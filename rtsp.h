#ifndef RTSP_H
#define RTSP_H

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
    struct list_head list;
#define SESION_READY 0
#define SESION_PLAYING 1
#define SESION_IN_FREE 2
    int status;
    char session_id[32];
    pthread_t rtp_thread;
    pthread_t rtcp_thread;
    Uri *uri;
    uv_loop_t rtp_loop;
    uv_loop_t rtcp_loop;
    uv_udp_t rtp_handle;
    uv_udp_t rtcp_handle;
    struct sockaddr_in serv_rtp_addr;
    struct sockaddr_in clit_rtp_addr;
    struct sockaddr_in serv_rtcp_addr;
    struct sockaddr_in clit_rtcp_addr;
    struct bufferevent *bev;
} rtsp_session;

#endif
