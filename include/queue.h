#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>

#define QUEUE_SIZE 256

typedef struct {
        void *args;
} request_t;

typedef struct {
        bool shutdown;
        int head;
        int tail;
        int count;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        request_t req[QUEUE_SIZE];
} queue_t;

#ifdef __cplusplus
extern "C" {
#endif

void init_queue(queue_t *queue);
void shutdown_queue(queue_t *queue);
void destroy_queue(queue_t *queue);
int enqueue(queue_t *queue, void *args);
void *dequeue(queue_t *queue);
int get_queue_count(queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
