#ifndef RTSP_H
#define RTSP_H

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

#define MAX_URL_LENGTH 1024
#define RTSP_VERSION "RTSP/1.0"

struct status_code {
    char *code;
    char *phrase;
};

//do NOT alloc this cause never free
struct rtsp_request_header {
    char session_id[64];
    char *user_agent;
    char *transport;
    char *accept;
    char *range;
    int cseq;
};

struct rtsp_request {
    struct rtsp_request_header rh;
    struct bufferevent *bev;
    char version[64];
    int method;
    char *url;
};

extern int make_response(struct rtsp_request *rr, char **buf);
extern void make_error_reply(int code, int cseq, char **response);
extern int convert_rtsp_request(struct rtsp_request **rr, struct bufferevent *bev, char *buf, int len);
extern void release_rtsp_request(struct rtsp_request *rr);

#endif /* RTSP_H */
