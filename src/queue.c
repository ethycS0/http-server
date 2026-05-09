#include "queue.h"

static request_t request;
static queue_t queue = {.head = 0,
                        .tail = 0,
                        .empty = true,
                        .mutex = PTHREAD_MUTEX_INITIALIZER,
                        .cond = PTHREAD_COND_INITIALIZER,
                        .req = {{0}}};

int enqueue(void *args) {
        pthread_mutex_lock(&queue.mutex);

        if ((queue.head + 1) % QUEUE_SIZE == queue.tail) {
                pthread_mutex_unlock(&queue.mutex);
                return -1;
        }

        queue.empty = false;
        queue.head = (queue.head + 1) % QUEUE_SIZE;
        queue.req[queue.head].args = args;

        pthread_cond_signal(&queue.cond);
        pthread_mutex_unlock(&queue.mutex);

        return 0;
}

void *dequeue() {
        pthread_mutex_lock(&queue.mutex);

        while (queue.empty == true) {
                pthread_cond_wait(&queue.cond, &queue.mutex);
        }

        queue.tail = (queue.tail + 1) % QUEUE_SIZE;
        void *args = queue.req[queue.tail].args;

        if (queue.tail == queue.head) {
                queue.empty = true;
        }

        pthread_mutex_unlock(&queue.mutex);
        return args;
}

bool is_queue_empty() {
        pthread_mutex_lock(&queue.mutex);
        bool state = queue.empty;
        pthread_mutex_unlock(&queue.mutex);
        return state;
}
