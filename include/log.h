#ifndef LOG_H
#define LOG_H

#define LOG(fmt, ...)                                                                                                  \
        do {                                                                                                           \
                printf("[LOG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);                                   \
        } while (0)
#define ERR(fmt, ...)                                                                                                  \
        do {                                                                                                           \
                fprintf(stderr, "[ERR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);                          \
        } while (0)

#endif
