#ifndef COMMON_H
#define COMMON_H

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef std::pair<unsigned int, void*> KV;

#include <time.h>

#define msleep(x) \
    do { \
        struct timespec time_to_wait; \
        time_to_wait.tv_sec = x / 1000; \
        time_to_wait.tv_nsec = 1000 * 1000 * (x - time_to_wait.tv_sec * 1000); \
        nanosleep(&time_to_wait, NULL); \
    } while(0)

#endif /* COMMON_H */
