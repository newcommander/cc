#ifndef SESSION_H
#define SESSION_H

#include "list.h"

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
};

#endif
