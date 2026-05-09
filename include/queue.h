#ifndef QUEUE_H
#define QUEUE_H

// Create a Queue
// Queue Tasks Based on Requests (Read and Write data to socket | Parse HTTP requirements | Serve Pages )
// Create a Thread Pool
// Any free threads from the pool accept tasks from the queue
// Increase Threads till Max Threads based on Queue Size
// Decrease Threads till Min Threads based on Idle threads
// If MAX_THREADS Reached, close sockets, drop clients

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
