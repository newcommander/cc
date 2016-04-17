#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

static char active_addr[16];

void send_cb(uv_udp_send_t *req, int status)
{
    printf("send done, status: %d\n", status);
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = calloc(suggested_size, 1);
    buf->len = suggested_size;
}

void recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
    printf("recv:[%d] %s\n", nread, buf->base);
}

int main()
{
    uv_loop_t loop;
    uv_udp_t rtcp_sock;
    uv_udp_t rtcp_sock1;
    struct sockaddr_in addr;
    struct sockaddr_in addr1;
    int ret, port;
    memset(active_addr, 0, 16);
    memcpy(active_addr, "192.168.1.8", 11);

    ret = uv_loop_init(&loop);
    if (ret) {
        printf("%s: uv_loop_init failed\n", __func__);
        return -1;
    }

    ret = uv_udp_init(&loop, &rtcp_sock);
    if (ret) {
        printf("%s: uv_udp_init failed\n", __func__);
        goto out;
    }

    port = 1025;
    uv_ip4_addr(active_addr, port, &addr);
    while (ret = uv_udp_bind(&rtcp_sock, (const struct sockaddr*)&addr, 0)) {
        if (ret == UV_EBUSY)
            uv_ip4_addr(active_addr, ++port, &addr);
        else
            goto out;
    }
    if (ret != 0) {
        printf("%s: %s\n", __func__, uv_strerror(ret));
        goto out;
    }
    printf("first bind port: %d\n", port);
    ret = uv_udp_init(&loop, &rtcp_sock1);
    uv_ip4_addr(active_addr, port, &addr1);
    while (ret = uv_udp_bind(&rtcp_sock1, (const struct sockaddr*)&addr1, 0)) {
        if (ret == UV_EADDRINUSE)
            uv_ip4_addr(active_addr, ++port, &addr1);
        else
            goto out;
    }
    if (ret != 0) {
        printf("%s: %s\n", __func__, uv_strerror(ret));
        goto out;
    }
    printf("second bind port: %d\n", port);

    ret = uv_udp_recv_start(&rtcp_sock, alloc_cb, recv_cb);
    if (ret != 0) {
        printf("recive err: %s\n", uv_strerror(ret));
        goto out;
    }

    uv_run(&loop, UV_RUN_DEFAULT);

out:
    uv_loop_close(&loop);
    return 0;
}
