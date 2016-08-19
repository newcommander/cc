#ifndef COMMON_H
#define COMMON_H

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <uv.h>

#include <stdint.h>

#include <pthread.h>
#include <time.h>

#define msleep(x) \
    do { \
        struct timespec time_to_wait; \
        time_to_wait.tv_sec = x / 1000; \
        time_to_wait.tv_nsec = 1000 * 1000 * (x - time_to_wait.tv_sec * 1000); \
        nanosleep(&time_to_wait, NULL); \
    } while(0)

#define PACKET_BUFFER_SIZE 1*1024*1024

typedef void (*frame_operation)(void *arg);

typedef struct {
    char *title;
	char *sdp_string;
    frame_operation func;
} uri_entry;

#endif
