#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <pthread.h>
#include <linux/types.h>

#include "rtsp.h"

#define RTP_VERSION_MASK 0xc0000000
#define RTP_PADDING_MASK 0x20000000
#define RTP_EXTSION_MASK 0x10000000
#define RTP_CSRCCNT_MASK 0x0f000000
#define RTP_MARKER_MASK  0x00800000
#define RTP_PAYLOAD_MASK 0x007f0000
#define RTP_SEQNUMB_MASK 0x0000ffff

struct rtp_header {
    __u32 fix_head;
    __u32 timestamp;
    __u32 ssrc;
    __u32 csrc[15];
};

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
    printf("recv:[%d] %s\n", nread, buf->base);
}

void* rtp_dispatch(void *arg)
{
    rtsp_session *rs = (rtsp_session*)arg;
    int ret;

	printf("rtp: %s\n", rs->session_id);

    ret = uv_udp_recv_start(&rs->rtp_handle, alloc_cb, recv_cb);
    if (ret != 0) {
        printf("%s: recive err: %s\n", __func__, uv_strerror(ret));
        goto out;
    }

    uv_run(&rs->rtp_loop, UV_RUN_DEFAULT);

out:
    uv_loop_close(&rs->rtp_loop);
    return 0;
}
