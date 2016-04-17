#ifndef RTSP_H
#define RTSP_H

#include <pthread.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <uv.h>

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

status_code response_code[] = {
    { "100", "Continue" },
    { "200", "OK" },
    { "201", "Created" },
    { "250", "Low on Storage Space" },
    { "300", "Multiple Choices" },
    { "301", "Moved Permanently" },
    { "302", "Moved Temporarily" },
    { "303", "See Other" },
    { "304", "Not Modified" },
    { "305", "Use Proxy" },
    { "400", "Bad Request" },
    { "401", "Unauthorized" },
    { "402", "Payment Required" },
    { "403", "Forbidden" },
    { "404", "Not Found" },
    { "405", "Method Not Allowed" },
    { "406", "Not Acceptable" },
    { "407", "Proxy Authentication Required" },
    { "408", "Request Time-out" },
    { "410", "Gone" },
    { "411", "Length Required" },
    { "412", "Precondition Failed" },
    { "413", "Request Entity Too Large" },
    { "414", "Request-URI Too Large" },
    { "415", "Unsupported Media Type" },
    { "451", "Parameter Not Understood" },
    { "452", "Conference Not Found" },
    { "453", "Not Enough Bandwidth" },
    { "454", "Session Not Found" },
    { "455", "Method Not Valid in This State" },
    { "456", "Header Field Not Valid for Resource" },
    { "457", "Invalid Range" },
    { "458", "Parameter Is Read-Only" },
    { "459", "Aggregate operation not allowed" },
    { "460", "Only aggregate operation allowed" },
    { "461", "Unsupported transport" },
    { "462", "Destination unreachable" },
    { "500", "Internal Server Error" },
    { "501", "Not Implemented" },
    { "502", "Bad Gateway" },
    { "503", "Service Unavailable" },
    { "504", "Gateway Time-out" },
    { "505", "RTSP Version not supported" },
    { "551", "Option not supported" },
    { NULL, NULL }
};

typedef struct {
    pthread_mutex_t ref_mutex;
    int ref;
    char *url;
} rtsp_uri;

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
    rtsp_uri *uri;
    uv_loop_t rtp_loop;
    uv_loop_t rtcp_loop;
    uv_udp_t rtp_handle;
    uv_udp_t rtcp_handle;
    struct sockaddr_in rtp_addr;
    struct sockaddr_in rtcp_addr;
    char session_id[32];
    struct bufferevent *bev;
} rtsp_session;

#endif
