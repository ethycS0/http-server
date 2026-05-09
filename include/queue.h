#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
#include <atomic>
using std::atomic_bool;
using std::atomic_load;
using std::atomic_store;
#else
#include <stdatomic.h>
#endif

#include <pthread.h>
#include <stdbool.h>

#define QUEUE_SIZE 16

typedef struct {
        void *args;
} request_t;

typedef struct {
        atomic_bool init;
        atomic_bool shutdown;
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

int init_queue(queue_t *queue);
int deinit_queue(queue_t *queue);
int enqueue(queue_t *queue, void *args);
void *dequeue(queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
