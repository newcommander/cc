#ifndef COMMON_H
#define COMMON_H

#include <time.h>

#define msleep(x) \
    do { \
        struct timespec time_to_wait; \
        time_to_wait.tv_sec = x / 1000; \
        time_to_wait.tv_nsec = 1000 * 1000 * (x - time_to_wait.tv_sec * 1000); \
        nanosleep(&time_to_wait, NULL); \
    } while(0)

#define PACKET_BUFFER_SIZE 1*1024*1024

#endif /* COMMON_H */
