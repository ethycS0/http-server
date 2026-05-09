#ifndef THREAD_POOL_H
#define THREAD_POOL_H

int init_workers(void *(*routine)(void *));
void deinit_workers();
void scale_threads(int load);

#endif
