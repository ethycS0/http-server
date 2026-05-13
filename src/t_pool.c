#include "t_pool.h"
#include "log.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_WORKERS 16
#define MIN_WORKERS 4

typedef struct thread_node_t {
        thread_args_t thread;
        struct thread_node_t *prev;
} thread_node_t;

static void *(*worker_routine)(void *);
static void *worker_args;

static pthread_mutex_t thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static thread_node_t *thread_pool_head = NULL;
pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;

static atomic_int thread_count = 0;
static atomic_bool init_pool = false;

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

                atomic_store(&new_thread->thread.t_state.state, WORKER_IDLE);
                atomic_fetch_add(&thread_count, 1);
                if (pthread_create(&new_thread->thread.t_state.tid, NULL, worker_routine, &new_thread->thread) != 0) {
                        ERR("Failed to create thread.");
                        atomic_fetch_sub(&thread_count, 1);
                        free(new_thread);
                        pthread_mutex_unlock(&thread_pool_mutex);
                        return i;
                }
                pthread_detach(new_thread->thread.t_state.tid);

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
                if (atomic_load(&current->thread.t_state.state) == WORKER_IDLE) {
                        pthread_cancel(current->thread.t_state.tid);

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

int init_workers(void *(*routine)(void *), void *args) {
        if (atomic_load(&init_pool) == true) {
                ERR("Thread pool already initialized.");
                return -1;
        }

        worker_routine = routine;
        worker_args = args;
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

void deinit_workers() {
        if (atomic_load(&init_pool) == false) {
                LOG("Thread pool is already uninitialized.");
                return;
        }
        atomic_store(&init_pool, false);

        pthread_mutex_lock(&thread_pool_mutex);

        thread_node_t *current = thread_pool_head;
        int killed_count = 0;

        while (current != NULL) {
                pthread_cancel(current->thread.t_state.tid);
                thread_node_t *node_to_remove = current;
                current = current->prev;
                free(node_to_remove);

                killed_count++;
        }

        thread_pool_head = NULL;
        atomic_store(&thread_count, 0);

        pthread_mutex_unlock(&thread_pool_mutex);

        LOG("Thread pool completely dismantled. %d workers cancelled.", killed_count);
}

void scale_threads(int load) {
        if (atomic_load(&init_pool) == false) {
                ERR("Workers not initialized.");
                return;
        }

        int current_threads = get_current_threads();

        if (load > current_threads && current_threads < MAX_WORKERS) {
                add_threads((load - current_threads));
                LOG("Dynamic Scaling Complete. Current Workers: %d", get_current_threads());
        } else if (load < current_threads && current_threads > MIN_WORKERS) {
                remove_threads((current_threads - load), false);
                LOG("Dynamic Scaling Complete. Current Workers: %d", get_current_threads());
        }
}

int get_current_threads() { return atomic_load(&thread_count); }

int get_current_free_threads() {
        if (atomic_load(&init_pool) == false) {
                ERR("Workers not initialized.");
                return -1;
        }

        thread_node_t *current = thread_pool_head;
        int free_threads = 0;

        while (current != NULL) {
                if (atomic_load(&current->thread.t_state.state) == WORKER_IDLE) {
                        free_threads += 1;
                }
                current = current->prev;
        }

        return free_threads;
}
