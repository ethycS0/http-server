#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdatomic.h>

enum thread_state { WORKER_BUSY, WORKER_IDLE };

typedef struct thread_t {
        atomic_int state;
        pthread_t tid;
} thread_t;

typedef struct thread_args_t {
        void *custom_args;
        thread_t t_state;
} thread_args_t;

int get_current_threads();
int get_current_free_threads();
int init_workers(void *(*routine)(void *), void *args);
void deinit_workers();
void scale_threads(int load);

// Queue
//
// enqueue() dequeue() argument required is queue_t*
//
//
// Thread Pool
//
// scale_threads() {add / remove threads}
// all threads created -> worker function
// pass2 arguments -> actual function + Queue
//
// worker function
// takes arguemnts, runs a while loop, calls dequeue() and run actual function with dequeue args
//

#endif
