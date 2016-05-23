#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <pthread.h>

#include "rtsp.h"

#define RTP_VERSION_MASK 0xc0000000
#define RTP_PADDING_MASK 0x20000000
#define RTP_EXTSION_MASK 0x10000000
#define RTP_CSRCCNT_MASK 0x0f000000
#define RTP_MARKER_MASK  0x00800000
#define RTP_PAYLOAD_MASK 0x007f0000
#define RTP_SEQNUMB_MASK 0x0000ffff

struct rtp_header {
    uint32_t fix_head;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[15];
};

static void set_version(struct rtp_header *header)
{
    header->fix_head = (header->fix_head & ~RTP_VERSION_MASK) | (uint32_t)2 << 30;
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

static void convert_header(struct rtp_header *header, const char *base, int len)
{
	int csrc_count = 0, i;

	memset(header, 0, sizeof(*header));
	memcpy(header, base, sizeof(header) < len ? sizeof(header) : len);
	header->fix_head = ntohl(header->fix_head);
	header->timestamp = ntohl(header->timestamp);
	header->ssrc = ntohl(header->ssrc);

	csrc_count = (header->fix_head & RTP_CSRCCNT_MASK) >> 24;
	csrc_count = csrc_count < 15 ? csrc_count : 15;
	for (i = 0; i < csrc_count; i++) {
		header->csrc[i] = ntohl(header->csrc[i]);
	}
	// TODO: need to set payload position ? maybe rtp_server do not recive payload ?
}

static void recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
	struct rtp_header header;
	int payload_type = 0;
	uint16_t seq_num = 0;

	if (nread == 0)
		return;
	if (nread < 0) {
		printf("%s: recive error: %s\n", __func__, uv_strerror(nread));
		return;
	}

	convert_header(&header, buf->base, nread);

	if (((header.fix_head & RTP_VERSION_MASK) >> 30) != 2) {
		printf("%s: Bad RTP packet version matched\n", __func__);
		return;
	}

	if ((header.fix_head & RTP_PADDING_MASK) >> 29) ; // TODO

	if ((header.fix_head & RTP_EXTSION_MASK) >> 28) ; // TODO

	if ((header.fix_head & RTP_MARKER_MASK) >> 23) ; // TODO

	payload_type = (header.fix_head & RTP_PAYLOAD_MASK) >> 16;
	seq_num = header.fix_head & RTP_SEQNUMB_MASK;
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
