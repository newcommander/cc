#ifndef SESSION_H
#define SESSION_H

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

#include <pthread.h>

#include "list.h"
#include "uri.h"

struct session {
#define SESION_READY 0
#define SESION_PLAYING 1
#define SESION_IN_FREE 2
    int status;
    struct bufferevent *bev;
    struct list_head list;
    struct list_head uri_user_list;
    char session_id[32];
    pthread_t rtp_thread;
    pthread_t rtp_send_thread;
    pthread_t rtcp_thread;
    pthread_t rtcp_send_thread;
    struct Uri *uri;
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
    char encoder_name[128];
    AVCodecContext *cc;
    AVFrame *frame;
    int pts;
};

#include "encoder.h"

extern struct list_head session_list;

extern void session_destroy(struct session *se);
extern struct session *session_create(char *url, struct bufferevent *bev,
        int client_rtp_port, int client_rtcp_port);
#endif
