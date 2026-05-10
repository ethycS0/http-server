#include "queue.h"
#include "log.h"

#include <pthread.h>
#include <stdio.h>

void init_queue(queue_t *queue) {
        queue->head = 0;
        queue->tail = 0;
        queue->count = 0;
        queue->shutdown = false;

        pthread_mutex_init(&queue->mutex, NULL);
        pthread_cond_init(&queue->cond, NULL);
}

void shutdown_queue(queue_t *queue) {
        pthread_mutex_lock(&queue->mutex);
        queue->shutdown = true;
        pthread_cond_broadcast(&queue->cond);
        pthread_mutex_unlock(&queue->mutex);
}

void destroy_queue(queue_t *queue) {
        pthread_mutex_destroy(&queue->mutex);
        pthread_cond_destroy(&queue->cond);
}

int enqueue(queue_t *queue, void *args) {
        pthread_mutex_lock(&queue->mutex);

        if (queue->shutdown) {
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
        pthread_mutex_lock(&queue->mutex);

        while (queue->count == 0 && queue->shutdown == false) {
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
