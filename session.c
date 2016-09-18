#include <uv.h>

#include "common.h"
#include "session.h"

extern char active_addr[128];

static struct list_head session_list;
static pthread_mutex_t session_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
void del_session_from_rtp_list(struct session *se)
{
    del_from_rtp_list(&se->rtp_list);
    uv_udp_recv_stop(&se->rtp_handle);
    encoder_deinit(se);
}

int add_session_to_rtp_list(struct session *se)
{
    int ret;

    if (!se) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    memset(&se->rtp_pkt, 0, sizeof(se->rtp_pkt));

//    set_pkt_header(&se->rtcp_pkt);
//    se->rtcp_pkt.ssrc = htonl(se->uri->ssrc);

//    add_src_desc((struct sr_rtcp_pkt*)se->rtcp_pkt.extension, se);

    if (encoder_init(se) < 0)
        return -1;

    ret = uv_udp_recv_start(&se->rtp_handle, rtp_alloc_cb, rtp_recv_cb);
    if (ret != 0) {
        printf("%s: Starting RTP recive failed: %s\n", __func__, uv_strerror(ret));
        encoder_deinit(se);
        return -1;
    }

    add_to_rtp_list(&se->rtp_list);

    return 0;
}
*/

/* NOTE: please call bufferevent_free(se->bev) according your situation */
void session_destroy(struct session *se)
{
    if (!se) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }

    switch(se->status) {
    case SESION_IDLE:
        unref_uri(se->uri, &se->uri_user_list);
        se->status = SESION_IN_FREE;
        break;
    case SESION_PLAYING:
        del_session_from_rtp_list(se);
        del_session_from_rtcp_list(se);
        unref_uri(se->uri, &se->uri_user_list);
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

    // TODO: unbind RTP/RTCP udp port
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

void session_destroy_all()
{
    struct list_head *list_p = NULL;
    struct session *se = NULL;
    for (list_p = session_list.next; list_p != &session_list;) {
        se = list_entry(list_p, struct session, list);
        list_p = list_p->next;
        bufferevent_flush(se->bev, EV_WRITE, BEV_FLUSH);
        session_destroy(se);
    }
}

int clean_uri_users(struct Uri *uri)
{
    struct list_head *list_p = NULL;
    struct session *se = NULL;

    if (!uri) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    for (list_p = uri->user_list.next; list_p != &uri->user_list; ) {
        se = list_entry(list_p, struct session, uri_user_list);
        list_p = list_p->next;
        session_destroy(se);
    }

    return 0;
}

struct session *find_session_by_id(char *session_id)
{
    struct session *se = NULL, *p = NULL;
    if (!session_id)
        return NULL;
    pthread_mutex_lock(&session_list_mutex);
    list_for_each_entry(p, &session_list, list) {
        if (!strcmp(p->session_id, session_id)) {
            se = p;
            if (se->status == SESION_IN_FREE)
                se = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&session_list_mutex);
    return se;
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
    memset(clit_ip, 0, sizeof(clit_ip));
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

    ret = init_rtcp_handle(&se->rtcp_handle);
    if (ret != 0) {
        printf("%s: Init RTCP UDP handle failed: %s\n", __func__, uv_strerror(ret));
        goto init_rtcp_handle_failed;
    }
    ret = init_rtp_handle(&se->rtp_handle);
    if (ret != 0) {
        printf("%s: Init RTP UDP handle failed: %s\n", __func__, uv_strerror(ret));
        goto init_rtp_handle_failed;
    }

    port = 1025;
    ret = 1;
    while (ret) {
        if (port > 65534)
            goto bind_rtp_addr_failed;

        uv_ip4_addr(active_addr, port++, &se->serv_rtp_addr);
        ret = uv_udp_bind(&se->rtp_handle, (struct sockaddr*)&se->serv_rtp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret) {
            printf("%s: bind RTP addr failed\n", __func__);
            goto bind_rtp_addr_failed;
        }

        // FIXME: what if port for rtp is not in use but port for rtcp is in use ?? need unbind the former ??
        uv_ip4_addr(active_addr, port++, &se->serv_rtcp_addr);
        ret = uv_udp_bind(&se->rtcp_handle, (struct sockaddr*)&se->serv_rtcp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret) {
            printf("%s: bind RTCP addr failed\n", __func__);
            goto bind_rtcp_addr_failed;
        }

        break;
    }

    gettimeofday(&tv, NULL);
    snprintf(se->session_id, 9, "%04x%04x", (unsigned int)tv.tv_usec, (unsigned int)rand());

    ret = ref_uri(se->uri, &se->uri_user_list);
    if (ret < 0) {
        printf("%s: Cannot reference uri(%s)\n", __func__, se->uri->url);
        goto ref_uri_failed;
    }

    se->status = SESION_IDLE;

    pthread_mutex_lock(&session_list_mutex);
    list_add(&se->list, &session_list);
    pthread_mutex_unlock(&session_list_mutex);

    return se;

ref_uri_failed:
    ; // TODO: unbind RTCP port
bind_rtcp_addr_failed:
    ; // TODO: unbind RTP port
bind_rtp_addr_failed:
    uv_close((uv_handle_t*)&se->rtp_handle, NULL);
init_rtp_handle_failed:
    uv_close((uv_handle_t*)&se->rtcp_handle, NULL);
init_rtcp_handle_failed:
    session_destroy(se);
    return NULL;
}

void session_list_init()
{
    INIT_LIST_HEAD(&session_list);
}

