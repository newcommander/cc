#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>

#include "rtsp.h"

#define RTCP_VERSION_MASK 0xc0000000
#define RTCP_PADDING_MASK 0x20000000
#define RTCP_RPCOUNT_MASK 0x1f000000
#define RTCP_PKTTYPE_MASK 0x00ff0000
#define RTCP_LENGTH_MASK  0x0000ffff

struct sr_rtcp_pkt {
    uint32_t header;
    uint32_t ssrc;
    uint32_t ntp_timestamp_msw;
    uint32_t ntp_timestamp_lsw;
    uint32_t rtp_timestamp;
    uint32_t packet_count;
    uint32_t octet_count;
    // with no report blocks
    uint32_t extension[16]; // TODO: fixed size ?
};

static void clean_up(void *arg)
{
    void *res = NULL;
    int ret = 0;
    rtsp_session *rs = (rtsp_session*)arg;
    assert(rs);

    if (rs->rtcp_send_thread) {
        ret = pthread_cancel(rs->rtcp_send_thread);
        if (ret != 0)
            printf("%s: pthread_cancel for RTCP send_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?

        ret = pthread_join(rs->rtcp_send_thread, &res);
        if (ret != 0)
            printf("%s: pthread_join for RTCP send_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?
        rs->rtcp_send_thread = 0;
    }

    if (res == PTHREAD_CANCELED)
        ; // thread was canceled
    else
        ; // thread terminated normally

    if (rs->rtp_thread) {
        uv_udp_recv_stop(&rs->rtp_handle);
        uv_stop(&rs->rtp_loop);

        ret = pthread_join(rs->rtp_thread, &res);
        if (ret != 0)
            printf("%s: pthread_join for rtp_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?
        rs->rtp_thread = 0;
    }

    if (res == PTHREAD_CANCELED)
        ; // thread was canceled
    else
        ; // thread terminated normally
}

static void set_version(struct sr_rtcp_pkt *pkt)
{
    pkt->header = (pkt->header & ~RTCP_VERSION_MASK) | (uint32_t)2 << 30;
}

static void set_padding(struct sr_rtcp_pkt *pkt)
{
    pkt->header |= (uint32_t)RTCP_PADDING_MASK;
}

static void clear_padding(struct sr_rtcp_pkt *pkt)
{
    pkt->header &= (uint32_t)~RTCP_PADDING_MASK;
}

static void set_report_count(struct sr_rtcp_pkt *pkt, uint32_t count)
{
    count &= (uint32_t)0x0000001f;
    pkt->header = (pkt->header & ~RTCP_RPCOUNT_MASK) | count << 24;
}

static void set_pkt_type(struct sr_rtcp_pkt *pkt, uint32_t code)
{
    pkt->header = (pkt->header & ~RTCP_PKTTYPE_MASK) | code << 16;
}

static void set_pkt_length(struct sr_rtcp_pkt *pkt, uint32_t length)
{
    length &= (uint32_t)0x0000ffff;
    pkt->header = (pkt->header & ~RTCP_LENGTH_MASK) | length;
}

static void set_pkt_header(struct sr_rtcp_pkt *pkt)
{
    set_version(pkt);
    if (1)
        set_padding(pkt);
    clear_padding(pkt);
    set_report_count(pkt, 0);
    set_pkt_type(pkt, 200);
    set_pkt_length(pkt, 6);
    pkt->header = htonl(pkt->header);
}

static void set_ntp_timestamp(struct sr_rtcp_pkt *pkt, struct timeval *tv)
{
    if (!pkt || !tv)
        return;
    pkt->ntp_timestamp_msw = htonl(tv->tv_sec + 0x83AA7E80); // 0x83AA7E80 is the number of seconds from 1900 to 1970
    // lsw is a number in unit of 232 picosecond, 1s ~= 2^32 * 232ps
    pkt->ntp_timestamp_lsw = htonl((uint32_t)((double)tv->tv_usec * 1.0e-6 * (((uint64_t)1) << 32)));
}

uint32_t timeval_to_rtp_timestamp(struct timeval *tv, rtsp_session *rs)
{
    uint32_t increment;
    if (!tv || !rs)
        return (uint32_t)-1;
    increment = rs->samping_rate * tv->tv_sec;
    increment += (uint32_t)((tv->tv_usec / 1000000.0) * rs->samping_rate);
    return rs->timestamp_offset + increment;
}

static void set_rtp_timestamp(struct sr_rtcp_pkt *pkt, struct timeval *tv, rtsp_session *rs)
{
    if (!pkt)
        return;
    pkt->rtp_timestamp = htonl(timeval_to_rtp_timestamp(tv, rs));
}

static void set_source_count(void *pkt, uint32_t count)
{
    set_report_count((struct sr_rtcp_pkt*)pkt, count);
}

static void add_src_desc(struct sr_rtcp_pkt *pkt, rtsp_session *rs)
{
    set_version(pkt);
    clear_padding(pkt);
    set_source_count(pkt, 1);
    set_pkt_type(pkt, 202);
    set_pkt_length(pkt, 3);
    pkt->header = htonl(pkt->header);
    pkt->ssrc = htonl(rs->uri->ssrc);
    pkt->ntp_timestamp_msw = htonl(0x01026464);
    pkt->ntp_timestamp_lsw = htonl(0x00000000);
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

extern void* rtp_dispatch(void *arg);
void* send_dispatch(void *arg)
{
    rtsp_session *rs = (rtsp_session*)arg;
    struct sr_rtcp_pkt pkt;
    struct timeval tv;
    struct msghdr h;
    uv_buf_t uv_buf;
    ssize_t size;
    int retry;

    if (!rs)
        return NULL;

    memset(&pkt, 0, sizeof(pkt));

    rs->timestamp_offset = (uint32_t)rand();
    rs->samping_rate = 90000;
    rs->rtcp_interval = 1500;

    set_pkt_header(&pkt);
    pkt.ssrc = htonl(rs->uri->ssrc);

    gettimeofday(&tv, NULL);
    set_ntp_timestamp(&pkt, &tv);
    set_rtp_timestamp(&pkt, &tv, rs);
    pkt.packet_count = htonl(rs->packet_count);
    pkt.octet_count = htonl(rs->octet_count);
    add_src_desc((struct sr_rtcp_pkt*)pkt.extension, rs);

    uv_buf.base = (char*)&pkt;
    uv_buf.len = 44; // TODO: how to compute length ?

    retry = 5;
    while (retry--) {
        size = sendmsg(rs->rtcp_handle.io_watcher.fd, &h, 0);
        if (size >= 0)
            break;
    }

    if (pthread_create(&rs->rtp_thread, NULL, rtp_dispatch, rs) != 0) {
        printf("%s: creating rtp thread failed: %s\n", __func__, strerror(errno));
        rs->rtp_thread = 0;
        return NULL;
    }

    while (1) {
        msleep(rs->rtcp_interval);

        gettimeofday(&tv, NULL);
        set_ntp_timestamp(&pkt, &tv);
        set_rtp_timestamp(&pkt, &tv, rs);
        pkt.packet_count = htonl(rs->packet_count);
        pkt.octet_count = htonl(rs->octet_count);

        memset(&h, 0, sizeof(struct msghdr));
        h.msg_name = &rs->clit_rtcp_addr;
        h.msg_namelen = sizeof(struct sockaddr_in);
        h.msg_iov = (struct iovec*)&uv_buf;
        h.msg_iovlen = 1;

        // TODO: in fact, we don't use libuv when sending UDP packet, review it
        retry = 5;
        while (retry--) {
            size = sendmsg(rs->rtcp_handle.io_watcher.fd, &h, 0);
            if (size >= 0)
                break;
        }
    }

    return NULL;
}

static void timeout_cb(uv_timer_t *timer) {}

void* rtcp_dispatch(void *arg)
{
    rtsp_session *rs = (rtsp_session*)arg;
    uv_timer_t timeout;
    int ret = 0;

    pthread_cleanup_push(clean_up, rs);

    ret = uv_udp_recv_start(&rs->rtcp_handle, alloc_cb, recv_cb);
    if (ret != 0) {
        printf("%s: starting RTCP recive error: %s\n", __func__, uv_strerror(ret));
        goto out;
    }

    if (pthread_create(&rs->rtcp_send_thread, NULL, send_dispatch, rs) != 0) {
        printf("%s: creating RTCP send thread failed: %s\n", __func__, strerror(errno));
        uv_udp_recv_stop(&rs->rtcp_handle);
        rs->rtcp_send_thread = 0;
        goto out;
    }

    ret = uv_timer_init(&rs->rtcp_loop, &timeout);
    assert(ret == 0);
    ret = uv_timer_start(&timeout, timeout_cb, 500, 500);
    assert(ret == 0);
    uv_run(&rs->rtcp_loop, UV_RUN_DEFAULT);

    pthread_cleanup_pop(1);

out:
    uv_loop_close(&rs->rtcp_loop);
    return NULL;
}
