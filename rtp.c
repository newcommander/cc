#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "session.h"
#include "encoder.h"
#include "uri.h"

#define RTP_VERSION_MASK 0xc0000000
#define RTP_PADDING_MASK 0x20000000
#define RTP_EXTSION_MASK 0x10000000
#define RTP_CSRCCNT_MASK 0x0f000000
#define RTP_MARKER_MASK  0x00800000
#define RTP_PAYLOAD_MASK 0x007f0000
#define RTP_SEQNUMB_MASK 0x0000ffff

static uv_loop_t rtp_loop;
static uv_timer_t loop_alarm;
static struct list_head rtp_list;
static pthread_mutex_t rtp_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static int rtp_interval = 40;  // ms
static pthread_t send_thread = -1;

static void clean_up(void *arg)
{
	struct session *se = NULL;
    void *res = NULL;
    int ret = 0;

	ret = uv_timer_stop(&loop_alarm);
    assert(ret == 0);

	uv_stop(&rtp_loop);

	if (send_thread > 0) {
        ret = pthread_cancel(send_thread);
        if (ret != 0)
            printf("%s: pthread_cancel for RTP send_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?

        ret = pthread_join(send_thread, &res);
        if (ret != 0)
            printf("%s: pthread_join for RTP send_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?
        se->rtp_send_thread = -1;
    }

    if (res == PTHREAD_CANCELED)
        ; // thread was canceled
    else
        ; // thread terminated normally

	pthread_mutex_lock(&rtp_list_mutex);
    while (!list_empty(&rtp_list)) {
        se = list_first_entry(&rtp_list, struct session, rtp_list);
        list_del(&se->rtp_list);
        uv_udp_recv_stop(&se->rtp_handle);
        encoder_deinit(se);
    }
    pthread_mutex_unlock(&rtp_list_mutex);

    uv_loop_close(&se->rtp_loop);
}

static void set_version(struct rtp_pkt *pkt)
{
    pkt->header = (pkt->header & ~RTP_VERSION_MASK) | (uint32_t)2 << 30;
}

static void set_padding(struct rtp_pkt *pkt)
{
    pkt->header |= (uint32_t)RTP_PADDING_MASK;
}

static void clear_padding(struct rtp_pkt *pkt)
{
    pkt->header &= (uint32_t)~RTP_PADDING_MASK;
}

static void set_extension(struct rtp_pkt *pkt)
{
    pkt->header |= (uint32_t)RTP_EXTSION_MASK;
}

static void clear_extension(struct rtp_pkt *pkt)
{
    pkt->header &= (uint32_t)~RTP_EXTSION_MASK;
}

static void set_csrc_count(struct rtp_pkt *pkt, uint32_t csrc_count)
{
    csrc_count &= (uint32_t)0x000f;
    pkt->header = (pkt->header & ~RTP_CSRCCNT_MASK) | csrc_count << 24;
}

static void set_marker(struct rtp_pkt *pkt)
{
    pkt->header |= (uint32_t)RTP_MARKER_MASK;
}

static void clear_marker(struct rtp_pkt *pkt)
{
    pkt->header &= (uint32_t)~RTP_MARKER_MASK;
}

static void set_payload_type(struct rtp_pkt *pkt, uint32_t payload_type)
{
    payload_type = payload_type & (uint32_t)0x0000007f;
    pkt->header = (pkt->header & ~RTP_PAYLOAD_MASK) | payload_type << 16;
}

static void set_seq_num(struct rtp_pkt *pkt, uint16_t seq_num)
{
    pkt->header = (pkt->header & ~RTP_SEQNUMB_MASK) | (uint32_t)seq_num;
}

extern uint32_t timeval_to_rtp_timestamp(struct timeval *tv, struct session *se);
static void set_timestamp(struct rtp_pkt *pkt, struct session *se)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    pkt->timestamp = htonl(timeval_to_rtp_timestamp(&tv, se));
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = (container_of((uv_udp_t*)handle, struct session, rtp_handle))->rtp_recv_buf;
    buf->len = SESSION_RECV_BUF_SIZE;
}

void recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
    if (nread == 0)
        return;
    if (nread < 0) {
        printf("%s: recive error: %s\n", __func__, uv_strerror(nread));
        return;
    }
}

int init_rtp_handle(uv_udp_t *handle)
{
    return uv_udp_init(&rtp_loop, handle);
}

void del_from_rtp_list(struct list_head *list)
{
    pthread_mutex_lock(&rtp_list_mutex);
    list_del(list);
    pthread_mutex_unlock(&rtp_list_mutex);
}

void add_to_rtp_list(struct list_head *list)
{
    pthread_mutex_lock(&rtp_list_mutex);
    list_add_tail(list, &rtp_list);
    pthread_mutex_unlock(&rtp_list_mutex);
}

static void* send_dispatch(void *arg)
{
    struct session *se = NULL;
    struct rtp_pkt pkt;
    struct msghdr h;
    uv_buf_t uv_buf;
    ssize_t size = 0;
    uint16_t seq_num = 0;
    int retry = 0, len = 0, n = 0;
    unsigned char data[PACKET_BUFFER_SIZE];

    if (!se) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }

    memset(&pkt, 0, sizeof(pkt));
    set_version(&pkt);
    if (0) { set_padding(&pkt); set_extension(&pkt); }
    clear_padding(&pkt);
    clear_extension(&pkt);
    set_csrc_count(&pkt, 0);
    clear_marker(&pkt);
    set_payload_type(&pkt, 96);
    pkt.header = htonl(pkt.header);
    pkt.ssrc = htonl(se->uri->ssrc);

    uv_buf.base = (char*)&pkt;

    memset(&h, 0, sizeof(struct msghdr));
    h.msg_name = &se->clit_rtp_addr;
    h.msg_namelen = sizeof(struct sockaddr_in);
    h.msg_iov = (struct iovec*)&uv_buf;
    h.msg_iovlen = 1;

    while (1) { // send packets one by one
        msleep(rtp_interval); // TODO: solve sampling frequnce

        pkt.header = ntohl(pkt.header);
        set_seq_num(&pkt, seq_num++);
        pkt.header = htonl(pkt.header);
        set_timestamp(&pkt, se);

//        memset(data, 0, PACKET_BUFFER_SIZE);
        if (sample_frame(se, data, &len) < 0) {
            printf("%s: video encoding failed on uri: '%s', continue next frame\n", __func__, se->uri->url);
            continue;
        }

        n = 0;
        while (n < len) { // send a packet segment by segment, in size of 1400
            uv_buf.len = len - n > 1400 ? 1400 : len - n;
            memcpy(pkt.payload, data + n, uv_buf.len);
            n += uv_buf.len;
            uv_buf.len += 12; // RTP header
            if (n == len) {
                pkt.header = ntohl(pkt.header);
                set_marker(&pkt);
                pkt.header = htonl(pkt.header);
            }
            retry = 5;
            while (retry--) { // resend segment on failing
                size = sendmsg(se->rtp_handle.io_watcher.fd, &h, 0);
                if (size >= 0)
                    break;
            }
            se->packet_count++;
            se->octet_count += size;
            if (n == len) {
                pkt.header = ntohl(pkt.header);
                clear_marker(&pkt);
                pkt.header = htonl(pkt.header);
            }
        }
    }

    return NULL;
}

static void timeout_cb(uv_timer_t *timer) { pthread_testcancel(); }

void* rtp_dispatch(void *arg)
{
    int ret = 0;

	INIT_LIST_HEAD(&rtp_list);
	memset(&rtp_loop, 0, sizeof(rtp_loop));
    ret = uv_loop_init(&rtp_loop);
    if (ret != 0) {
        printf("%s: Init RTP UDP loop failed: %s\n", __func__, uv_strerror(ret));
        return NULL; // TODO: how to let the thread creator know this failing ??
    }

    if (pthread_create(&send_thread, NULL, send_dispatch, NULL) != 0) {
        printf("%s: Creating RTP send thread failed: %s\n", __func__, strerror(errno));
        uv_loop_close(&rtp_loop);
        send_thread = -1;
        return NULL;
    }

    ret = uv_timer_init(&rtp_loop, &loop_alarm);
    assert(ret == 0);
    ret = uv_timer_start(&loop_alarm, timeout_cb, 100, 100);
    assert(ret == 0);

    pthread_cleanup_push(clean_up, NULL);

	uv_run(&rtp_loop, UV_RUN_DEFAULT);

    pthread_cleanup_pop(1);

    return NULL;
}

