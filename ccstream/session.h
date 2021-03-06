#ifndef SESSION_H
#define SESSION_H

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <uv.h>

#include <pthread.h>

struct session;
struct Uri;

#include "list.h"
#include "uri.h"
#include "rtcp.h"
#include "rtp.h"

#define SESSION_RECV_BUF_SIZE 1024

struct session {
    char session_id[32];
#define SESSION_IDLE 0
#define SESSION_PLAYING 1
#define SESSION_IN_FREE 2
    int status;
    struct bufferevent *bev;
    struct list_head list;
    struct list_head uri_user_list;
    struct list_head rtcp_list;
    struct list_head rtp_list;
    char rtcp_recv_buf[SESSION_RECV_BUF_SIZE];
    char rtp_recv_buf[SESSION_RECV_BUF_SIZE];
    struct rtcp_sr_pkt rtcp_pkt;
    struct rtp_pkt rtp_pkt;
    struct Uri *uri;
    uv_udp_t rtp_handle;
    uv_udp_t rtcp_handle;
#define HANDLE_BUSY 0
#define HANDLE_CLOSING 1
#define HANDLE_CLOSED 2
    int rtp_handle_status;
    int rtcp_handle_status;
    struct sockaddr_in serv_rtp_addr;
    struct sockaddr_in clit_rtp_addr;
    struct sockaddr_in serv_rtcp_addr;
    struct sockaddr_in clit_rtcp_addr;
    uint32_t timestamp_offset;
    uint32_t samping_rate;
    uint32_t packet_count;
    uint32_t octet_count;
    uint16_t rtp_seq_num;
    char *encoder_name;
    AVCodecContext *cc;
    AVFrame *frame;
    int pts;
};

void session_list_init();
struct session *session_create(struct Uri *uri, struct bufferevent *bev,
        int client_rtp_port, int client_rtcp_port);
struct session *find_session_by_id(char *session_id);
void session_destroy(struct session *se);
void session_destroy_all();

#endif /* SESSION_H */

