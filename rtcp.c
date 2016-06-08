#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

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
    clear_padding(pkt);
    set_report_count(pkt, 0);
    set_pkt_type(pkt, 200);
    set_pkt_length(pkt, 6);
    pkt->header = htonl(pkt->header);
}

static uint64_t timeval_to_ntp(const struct timeval *tv, struct sr_rtcp_pkt *pkt)
{
//    uint64_t ret;
    uint32_t msw, lsw;
    msw = tv->tv_sec + 0x83AA7E80; // 0x83AA7E80 is the number of seconds from 1900 to 1970
    // lsw is a number in unit of 232 picosecond, 1s ~= 2^32 * 232ps
    lsw = (uint32_t)((double)tv->tv_usec * 1.0e-6 * (((uint64_t)1) << 32));
    return htonl(((uint64_t)msw) << 32) | htonl(lsw);
}

static void set_ntp_timestamp(struct sr_rtcp_pkt *pkt, struct timeval *tv)
{
    if (!pkt || !tv)
        return;
    uint32_t msw, lsw;
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
    set_report_count(pkt, 1);
    set_pkt_type(pkt, 202);
    set_pkt_length(pkt, 3);
    pkt->header = htonl(pkt->header);
    pkt->ssrc = htonl(rs->uri->ssrc);
    pkt->ntp_timestamp_msw = htonl(0x01026464);
    pkt->ntp_timestamp_lsw = htonl(0x00000000);
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = calloc(suggested_size, 1);
    buf->len = suggested_size;
}

static void send_cb(uv_udp_send_t *req, int status)
{
    printf("send done, status: %d\n", status);
}

static void recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
	if (nread == 0)
		return;
	if (nread < 0) {
		printf("%s: recive error: %s\n", __func__, uv_strerror(nread));
		return;
	}
    printf("recv:[%ld]\n", nread);
}

extern void* rtp_dispatch(void *arg);
void* send_dispatch(void *arg)
{
    struct sr_rtcp_pkt pkt;
    struct timeval tv;
    uv_buf_t uv_buf;
    uv_udp_send_t req;
    rtsp_session *rs = (rtsp_session*)arg;
    if (!rs)
        return NULL;
    memset(&pkt, 0, sizeof(pkt));

    rs->timestamp_offset = (uint32_t)rand();
    rs->samping_rate = 90000;
    rs->rtcp_interval = 1500;

    set_pkt_header(&pkt);
    pkt.ssrc = htonl(rs->uri->ssrc);

    while (1) {
        gettimeofday(&tv, NULL);
        set_ntp_timestamp(&pkt, &tv);
        set_rtp_timestamp(&pkt, &tv, rs);
        { // TODO: need a lock ?
        pkt.packet_count = htonl(rs->packet_count);
        pkt.octet_count = htonl(rs->octet_count);
        }
        add_src_desc((struct sr_rtcp_pkt*)pkt.extension, rs);
        uv_buf.base = (char*)&pkt;
        uv_buf.len = 44; // TODO: how to compute length ?

        int ret;
        ret = uv_udp_send(&req, &rs->rtcp_handle, &uv_buf, 1, (const struct sockaddr*)&rs->clit_rtcp_addr, send_cb);
        if (ret != 0) {
            printf("send failed: %s\n", uv_strerror(ret));
        }

        msleep(rs->rtcp_interval);
    }

    /*
    if (pthread_create(&rs->rtp_thread, NULL, rtp_dispatch, rs) != 0) {
        printf("%s: creating rtp thread failed: %s\n", __func__, strerror(errno));
        return NULL;
    }
    */

    return NULL;
}

void* rtcp_dispatch(void *arg)
{
    rtsp_session *rs = (rtsp_session*)arg;
    int ret;

    ret = uv_udp_recv_start(&rs->rtcp_handle, alloc_cb, recv_cb);
    if (ret != 0) {
        printf("%s: recive err: %s\n", __func__, uv_strerror(ret));
        goto out;
    }

    pthread_t send_thread;
    if (pthread_create(&send_thread, NULL, send_dispatch, rs) != 0) {
        printf("%s: creating rtcp send thread failed: %s\n", __func__, strerror(errno));
        return NULL;
    }

    uv_run(&rs->rtcp_loop, UV_RUN_DEFAULT);

    pthread_join(send_thread, NULL);

out:
    uv_loop_close(&rs->rtcp_loop);
    return 0;
}
