#ifndef RTSP_H
#define RTSP_H

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <uv.h>
#include <pthread.h>
#include <time.h>

#include "list.h"
#include "uri.h"

#define MTH_DESCRIBE        1
#define MTH_ANNOUNCE        2
#define MTH_GET_PARAMETER   3
#define MTH_OPTIONS         4
#define MTH_PAUSE           5
#define MTH_PLAY            6
#define MTH_RECORD          7
#define MTH_REDIRECT        8
#define MTH_SETUP           9
#define MTH_SET_PARAMETER   10
#define MTH_TEARDOWN        11

#define RTSP_VERSION "RTSP/1.0"
#define PACKET_BUFFER_SIZE 1*1024*1024

#define msleep(x) \
    do { \
        struct timespec time_to_wait; \
        time_to_wait.tv_sec = x / 1000; \
        time_to_wait.tv_nsec = 1000 * 1000 * (x - time_to_wait.tv_sec * 1000); \
        nanosleep(&time_to_wait, NULL); \
    } while(0)

typedef struct {
    char *code;
    char *phrase;
} status_code;

//do NOT alloc this cause never free
typedef struct {
    int cseq;
    char *accept;
    char *user_agent;
	char *transport;
    char *range;
    char session_id[33];
} rtsp_request_header;

typedef struct {
    int method;
    char *url;
    char version[33];
    rtsp_request_header rh;
    struct bufferevent *bev;
} rtsp_request;

typedef struct {
    struct bufferevent *bev;
    struct list_head list;
#define SESION_READY 0
#define SESION_PLAYING 1
#define SESION_IN_FREE 2
    int status;
    char session_id[32];
    pthread_t rtp_thread;
    pthread_t rtp_send_thread;
    pthread_t rtcp_thread;
    pthread_t rtcp_send_thread;
    Uri *uri;
    uv_loop_t rtp_loop;
    uv_loop_t rtcp_loop;
    uv_udp_t rtp_handle;
    uv_udp_t rtcp_handle;
    struct sockaddr_in serv_rtp_addr;
    struct sockaddr_in clit_rtp_addr;
    struct sockaddr_in serv_rtcp_addr;
    struct sockaddr_in clit_rtcp_addr;
    uint32_t timestamp_offset;
    uint32_t samping_rate;
    uint32_t packet_count;
    uint32_t octet_count;
    int rtcp_interval;  // ms
    AVCodecContext *cc;
    AVFrame *frame;
    int pts;
} rtsp_session;

#endif
