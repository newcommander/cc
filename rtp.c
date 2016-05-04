#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <pthread.h>

#include "rtsp.h"

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = calloc(suggested_size, 1);
    buf->len = suggested_size;
}

void send_cb(uv_udp_send_t *req, int status)
{
    printf("send done, status: %d\n", status);
}

void recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
    printf("recv:[%d] %s\n", nread, buf->base);
}

void* rtp_dispatch(void *arg)
{
    rtsp_session *rs = (rtsp_session*)arg;
    int ret;

	printf("rtp: %s\n", rs->session_id);
    return NULL;

    ret = uv_udp_recv_start(&rs->rtp_handle, alloc_cb, recv_cb);
    if (ret != 0) {
        printf("recive err: %s\n", uv_strerror(ret));
        goto out;
    }

    uv_run(&rs->rtp_loop, UV_RUN_DEFAULT);

out:
    uv_loop_close(&rs->rtp_loop);
    return 0;
}
