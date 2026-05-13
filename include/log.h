#ifndef LOG_H
#define LOG_H

#include <pthread.h>

extern pthread_mutex_t debug_mutex;

#define LOG(fmt, ...)                                                                                                  \
        do {                                                                                                           \
                pthread_mutex_lock(&debug_mutex);                                                                      \
                printf("[LOG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);                                   \
                pthread_mutex_unlock(&debug_mutex);                                                                    \
        } while (0)

#define ERR(fmt, ...)                                                                                                  \
        do {                                                                                                           \
                pthread_mutex_lock(&debug_mutex);                                                                      \
                fprintf(stderr, "[ERR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);                          \
                pthread_mutex_unlock(&debug_mutex);                                                                    \
        } while (0)

#endif
