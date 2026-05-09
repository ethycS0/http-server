#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>

#define QUEUE_SIZE 16

typedef struct request_t {
        void *args;
} request_t;

typedef struct queue_t {
        int head;
        int tail;
        bool empty;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        request_t req[QUEUE_SIZE];
} queue_t;

int enqueue(void *args);
void *dequeue();
bool is_queue_empty();

#endif
