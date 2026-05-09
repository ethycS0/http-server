#include "queue.h"
#include "log.h"

#include <pthread.h>
#include <stdio.h>

int init_queue(queue_t *queue) {
        if (atomic_load(&queue->init) == true) {
                ERR("Queue already Initialized.");
                return -1;
        }

        queue->head = 0;
        queue->tail = 0;
        queue->count = 0;

        pthread_mutex_init(&queue->mutex, NULL);
        pthread_cond_init(&queue->cond, NULL);

        atomic_store(&queue->shutdown, false);
        atomic_store(&queue->init, true);

        return 0;
}

int deinit_queue(queue_t *queue) {
        if (atomic_load(&queue->init) == false) {
                ERR("Queue not initialized.");
                return -1;
        }

        pthread_mutex_lock(&queue->mutex);

        atomic_store(&queue->shutdown, true);
        atomic_store(&queue->init, false);
        pthread_cond_broadcast(&queue->cond);
        pthread_mutex_unlock(&queue->mutex);

        pthread_mutex_destroy(&queue->mutex);
        pthread_cond_destroy(&queue->cond);

        return 0;
}

int enqueue(queue_t *queue, void *args) {
        if (atomic_load(&queue->init) == false) {
                ERR("Queue not initialized.");
                return -1;
        }

        pthread_mutex_lock(&queue->mutex);

        if (atomic_load(&queue->shutdown)) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        if (queue->count >= QUEUE_SIZE) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        queue->req[queue->head].args = args;
        queue->head = (queue->head + 1) % QUEUE_SIZE;
        queue->count += 1;

        pthread_cond_signal(&queue->cond);
        pthread_mutex_unlock(&queue->mutex);

        return 0;
}

void *dequeue(queue_t *queue) {
        if (atomic_load(&queue->init) == false) {
                ERR("Queue not initialized.");
                return NULL;
        }

        pthread_mutex_lock(&queue->mutex);

        while (queue->count == 0 && !atomic_load(&queue->shutdown)) {
                pthread_cond_wait(&queue->cond, &queue->mutex);
        }

        if (queue->count == 0) {
                pthread_mutex_unlock(&queue->mutex);
                return NULL;
        }

        void *args = queue->req[queue->tail].args;
        queue->tail = (queue->tail + 1) % QUEUE_SIZE;
        queue->count -= 1;

        pthread_mutex_unlock(&queue->mutex);
        return args;
}
