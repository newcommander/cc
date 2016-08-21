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

struct rtp_pkt {
    uint32_t header;
    uint32_t timestamp;
    uint32_t ssrc;
    uint8_t payload[1400];
};

static void clean_up(void *arg)
{
    void *res = NULL;
    int ret = 0;
    struct session *se = (struct session*)arg;
    assert(se);

    if (se->rtp_send_thread) {
        ret = pthread_cancel(se->rtp_send_thread);
        if (ret != 0)
            printf("%s: pthread_cancel for RTP send_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?

        ret = pthread_join(se->rtp_send_thread, &res);
        if (ret != 0)
            printf("%s: pthread_join for RTP send_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?
        se->rtp_send_thread = 0;
    }

    if (res == PTHREAD_CANCELED)
        ; // thread was canceled
    else
        ; // thread terminated normally
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

static char slab[65536];
static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = slab;
    buf->len = sizeof(slab);
}

static void recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
    if (nread == 0)
        return;
    if (nread < 0) {
        printf("%s: recive error: %s\n", __func__, uv_strerror(nread));
        return;
    }
}

static void* send_dispatch(void *arg)
{
    struct session *se = (struct session*)arg;
    struct rtp_pkt pkt;
    struct msghdr h;
    uv_buf_t uv_buf;
    ssize_t size = 0;
    uint16_t seq_num = 0;
    int retry = 0, len = 0, n = 0;
    unsigned char data[PACKET_BUFFER_SIZE];

    if (!se)
        return NULL;

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
        msleep(40); // TODO: solve sampling frequnce

        pkt.header = ntohl(pkt.header);
        set_seq_num(&pkt, seq_num++);
        pkt.header = htonl(pkt.header);
        set_timestamp(&pkt, se);

        memset(data, 0, PACKET_BUFFER_SIZE);
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

static void timeout_cb(uv_timer_t *timer) {}

void* rtp_dispatch(void *arg)
{
    struct session *se = (struct session*)arg;
    uv_timer_t timeout;
    int ret;

    if (encoder_init(se) < 0)
        return NULL;

    pthread_cleanup_push(clean_up, se);

    ret = uv_udp_recv_start(&se->rtp_handle, alloc_cb, recv_cb);
    if (ret != 0) {
        printf("%s: starting RTP recive error: %s\n", __func__, uv_strerror(ret));
        goto out;
    }

    if (pthread_create(&se->rtp_send_thread, NULL, send_dispatch, se) != 0) {
        printf("%s: creating RTP send thread failed: %s\n", __func__, strerror(errno));
        uv_udp_recv_stop(&se->rtp_handle);
        se->rtp_send_thread = 0;
        goto out;
    }

    ret = uv_timer_init(&se->rtp_loop, &timeout);
    assert(ret == 0);
    ret = uv_timer_start(&timeout, timeout_cb, 500, 500);
    assert(ret == 0);
    uv_run(&se->rtp_loop, UV_RUN_DEFAULT);

    pthread_cleanup_pop(1);

    encoder_deinit(se);

out:
    uv_loop_close(&se->rtp_loop);
    return NULL;
}
