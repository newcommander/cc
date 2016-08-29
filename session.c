#include <uv.h>

#include "common.h"
#include "session.h"

extern char active_addr[128];

struct list_head session_list;
pthread_mutex_t session_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/* NOTE: please call bufferevent_free(se->bev) according your situation */
void session_destroy(struct session *se)
{
    void *res = NULL;
    int ret = 0, retry;

    if (!se)
        return;

    switch(se->status) {
    case SESION_READY:
        unref_uri(se->uri, &se->uri_user_list);
        se->status = SESION_IN_FREE;
        break;
    case SESION_PLAYING:
        unref_uri(se->uri, &se->uri_user_list);
        if (se->rtcp_thread) {
            uv_udp_recv_stop(&se->rtcp_handle);
            uv_stop(&se->rtcp_loop);
            ret = pthread_join(se->rtcp_thread, &res);
            if (ret != 0)
                printf("%s: pthread_join for rtcp_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?
            se->rtcp_thread = 0;
        }
        se->status = SESION_IN_FREE;
        break;
    case SESION_IN_FREE:
		break;
    }

    if (se->status != SESION_IN_FREE) {
        printf("%s: Could not destroy session on %s\n", __func__, se->uri->url);
        return;
    }
    pthread_mutex_lock(&session_list_mutex);
    list_del(&se->list);
    pthread_mutex_unlock(&session_list_mutex);

    retry = 10;
    while (retry--) {
        if (uv_loop_close(&se->rtcp_loop) != 0)
            msleep(200);
        else
            break;
    }
    retry = 10;
    while (retry--) {
        if (uv_loop_close(&se->rtp_loop) != 0)
            msleep(200);
        else
            break;
    }

	se->bev->wm_read.private_data = NULL;
    bufferevent_free(se->bev);

    if (se->cc) {
        if (avcodec_is_open(se->cc))
            avcodec_close(se->cc);
        av_free(se->cc);
        se->cc = NULL;
    }
    if (se->frame) {
        if (se->frame->data[0])
            av_freep(&se->frame->data[0]);
        av_frame_free(&se->frame);
        se->frame = NULL;
    }

    free(se);
}

struct session *session_create(char *url, struct bufferevent *bev, int client_rtp_port, int client_rtcp_port)
{
	struct session *se = NULL;
    struct Uri *uri = NULL;
    struct timeval tv;
    struct sockaddr clit_addr;
    struct sockaddr_in *clit_addr_in;
    socklen_t addr_len = sizeof(struct sockaddr);
    int port, ret;
    char clit_ip[16];

    if (!url || !bev || client_rtp_port < 1025 || client_rtcp_port < 1025) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }

    uri = find_uri(url);
    if (!uri) {
        printf("%s: No URI with url: %s found\n", __func__, url);
        return NULL;
    }

    ret = getpeername(bev->ev_read.ev_fd, &clit_addr, &addr_len);
    if (ret != 0) {
        printf("%s: getpername failed: %s\n", __func__, strerror(errno));
        return NULL;
    }

//    if (clit_addr.sa_family != AF_INET) {
//        printf("%s: Only serves on IPv4\n", __func__);
//        release_uri(uri);
//        return NULL;
//    }

	clit_addr_in = (struct sockaddr_in*)&clit_addr;
    memset(clit_ip, 0, 16);
    snprintf(clit_ip, addr_len < 16 ? addr_len : 16, "%s", inet_ntoa(clit_addr_in->sin_addr));

    se = (struct session*)calloc(1, sizeof(*se));
    if (!se) {
        printf("%s: calloc failed\n", __func__);
        return NULL;
    }
    se->uri = uri;
    se->bev = bev;
    se->bev->wm_read.private_data = se;
    uv_ip4_addr(clit_ip, client_rtp_port, &se->clit_rtp_addr);
    uv_ip4_addr(clit_ip, client_rtcp_port, &se->clit_rtcp_addr);

    ret = uv_loop_init(&se->rtp_loop);
    if (ret != 0) {
        printf("%s: Init RTP UDP loop failed: %s\n", __func__, uv_strerror(ret));
        goto failed_rtp_loop_init;
    }
    ret = uv_loop_init(&se->rtcp_loop);
    if (ret != 0) {
        printf("%s: Init RTCP UDP loop failed: %s\n", __func__, uv_strerror(ret));
        goto failed_rtcp_loop_init;
    }

    ret = uv_udp_init(&se->rtp_loop, &se->rtp_handle);
    if (ret != 0) {
        printf("%s: Init RTP UDP handle failed: %s\n", __func__, uv_strerror(ret));
        goto failed;
    }
    ret = uv_udp_init(&se->rtcp_loop, &se->rtcp_handle);
    if (ret != 0) {
        printf("%s: Init RTCP UDP handle failed: %s\n", __func__, uv_strerror(ret));
        goto failed;
    }

    port = 1025;
    ret = 1;
    while (ret) {
        uv_ip4_addr(active_addr, port++, &se->serv_rtp_addr);
        ret = uv_udp_bind(&se->rtp_handle, (struct sockaddr*)&se->serv_rtp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret)
            goto failed;
		// FIXME: what if port for rtp is not in use but port for rtcp is in use ?? need unbind the former ??
        uv_ip4_addr(active_addr, port++, &se->serv_rtcp_addr);
        ret = uv_udp_bind(&se->rtcp_handle, (struct sockaddr*)&se->serv_rtcp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret)
            goto failed;

        if (port > 65534)
            goto failed;

        break;
    }

	gettimeofday(&tv, NULL);
    snprintf(se->session_id, 9, "%04x%04x", (unsigned int)tv.tv_usec, (unsigned int)rand());

    ret = ref_uri(se->uri, &se->uri_user_list);
    if (ret < 0) {
        printf("%s: Cannot reference uri(%s)\n", __func__, se->uri->url);
        goto failed;
    }

    //TODO: how to decied encoder_name ??
    snprintf(se->encoder_name, 6, "h264");
    se->status = SESION_READY;

    pthread_mutex_lock(&session_list_mutex);
    list_add(&se->list, &session_list);
    pthread_mutex_unlock(&session_list_mutex);

    return se;

// failed_ref: never here
    unref_uri(uri, &se->uri_user_list);
failed:
    uv_loop_close(&se->rtcp_loop);
failed_rtcp_loop_init:
    uv_loop_close(&se->rtp_loop);
failed_rtp_loop_init:
    session_destroy(se);
    return NULL;
}

