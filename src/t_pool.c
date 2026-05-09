#include "t_pool.h"
#include "log.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_WORKERS 16
#define MIN_WORKERS 4

enum thread_state { WORKER_BUSY, WORKER_IDLE };

typedef struct thread_t {
        atomic_int state;
        pthread_t tid;
} thread_t;

typedef struct thread_node_t {
        thread_t thread;
        struct thread_node_t *prev;
} thread_node_t;

static pthread_mutex_t thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_int thread_count = 0;
static void *(*worker_routine)(void *);
static atomic_bool init_pool = false;
static thread_node_t *thread_pool_head = NULL;

int add_threads(int count) {
        pthread_mutex_lock(&thread_pool_mutex);

        if (count + thread_count > MAX_WORKERS) {
                count = MAX_WORKERS - thread_count;
        }

        for (int i = 0; i < count; i++) {
                thread_node_t *new_thread = malloc(sizeof(thread_node_t));
                if (new_thread == NULL) {
                        ERR("Failed to allocate memory.");
                        pthread_mutex_unlock(&thread_pool_mutex);
                        return i;
                }

                atomic_store(&new_thread->thread.state, WORKER_IDLE);
                atomic_fetch_add(&thread_count, 1);
                if (pthread_create(&new_thread->thread.tid, NULL, worker_routine, NULL) != 0) {
                        ERR("Failed to create thread.");
                        atomic_fetch_sub(&thread_count, 1);
                        free(new_thread);
                        pthread_mutex_unlock(&thread_pool_mutex);
                        return i;
                }

                new_thread->prev = thread_pool_head;
                thread_pool_head = new_thread;
        }

        pthread_mutex_unlock(&thread_pool_mutex);
        return count;
}

void remove_threads(int count, bool force_cleanup) {
        pthread_mutex_lock(&thread_pool_mutex);

        thread_node_t *current = thread_pool_head;
        thread_node_t *prev_thread = NULL;
        int removed_count = 0;

        if (thread_count - count < MIN_WORKERS && force_cleanup == false) {
                count = thread_count - MIN_WORKERS;
        }

        while (current != NULL && removed_count < count) {
                if (atomic_load(&current->thread.state) == WORKER_IDLE) {
                        pthread_cancel(current->thread.tid);

                        thread_node_t *node = current;
                        if (prev_thread == NULL) {
                                thread_pool_head = current->prev;
                        } else {
                                prev_thread->prev = current->prev;
                        }
                        current = current->prev;
                        free(node);
                        removed_count++;
                } else {
                        prev_thread = current;
                        current = current->prev;
                }
        }

        if (removed_count > 0) {
                ERR("Removed %d threads. Others Busy | Empty.", removed_count);
        }

        thread_count = thread_count - removed_count;

        pthread_mutex_unlock(&thread_pool_mutex);
}

int init_workers(void *(*routine)(void *)) {
        if (atomic_load(&init_pool) == true) {
                ERR("Workers already initialized.");
                return -1;
        }

        worker_routine = routine;
        int workers = add_threads(MIN_WORKERS);
        if (workers == MIN_WORKERS) {
                LOG("Workers started successfully.");
                atomic_store(&init_pool, true);
                return 0;
        } else {
                remove_threads(workers, true);
                ERR("Failed to Initialize Workers.");
                return -1;
        }
}

void scale_threads(int load) {
        int current_threads = atomic_load(&thread_count);
        if (atomic_load(&init_pool) == false) {
                ERR("Workers not initialized.");
                return;
        }

        if (load > current_threads && current_threads < MAX_WORKERS) {
                add_threads((load - current_threads));
                LOG("Dynamic Scaling Complete. Current Workers: %d", atomic_load(&thread_count));
        } else if (load < current_threads && current_threads > MIN_WORKERS) {
                remove_threads((current_threads - load), false);
                LOG("Dynamic Scaling Complete. Current Workers: %d", atomic_load(&thread_count));
        }
}
