#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <pthread.h>

#include "rtsp.h"

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
    printf("recv:[%d]\n", nread);
}

void* rtcp_dispatch(void *arg)
{
    rtsp_session *rs = (rtsp_session*)arg;
    int ret;

	printf("rtcp: %s\n", rs->session_id);

    ret = uv_udp_recv_start(&rs->rtcp_handle, alloc_cb, recv_cb);
    if (ret != 0) {
        printf("%s: recive err: %s\n", __func__, uv_strerror(ret));
        goto out;
    }

    uv_run(&rs->rtcp_loop, UV_RUN_DEFAULT);

out:
    uv_loop_close(&rs->rtcp_loop);
    return 0;
}
