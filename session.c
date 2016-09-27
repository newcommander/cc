#include <uv.h>

#include "common.h"
#include "session.h"

char active_addr[128];

static struct list_head session_list;
static pthread_mutex_t session_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void session_destroy(struct session *se)
{
    if (!se) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }

    switch(se->status) {
    case SESSION_IDLE:
        unref_uri(se);
        break;
    case SESSION_PLAYING:
        del_session_from_rtp_list(se);
        del_session_from_rtcp_list(se);
        unref_uri(se);
        break;
    case SESSION_IN_FREE:
        break;
    }

    se->status = SESSION_IN_FREE;

    pthread_mutex_lock(&session_list_mutex);
    list_del(&se->list);
    pthread_mutex_unlock(&session_list_mutex);

    if ((se->rtp_handle_status != HANDLE_CLOSING) && (se->rtp_handle_status != HANDLE_CLOSED)) {
        uv_close((uv_handle_t*)&se->rtp_handle, rtp_handle_close_cb);
        se->rtp_handle_status = HANDLE_CLOSING;
    }
    if ((se->rtcp_handle_status != HANDLE_CLOSING) && (se->rtcp_handle_status != HANDLE_CLOSED)) {
        uv_close((uv_handle_t*)&se->rtcp_handle, rtcp_handle_close_cb);
        se->rtcp_handle_status = HANDLE_CLOSING;
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

    while ((se->rtp_handle_status != HANDLE_CLOSED) || (se->rtcp_handle_status != HANDLE_CLOSED)) msleep(10);

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

struct session *find_session_by_id(char *session_id)
{
    struct session *se = NULL, *p = NULL;
    if (!session_id)
        return NULL;
    pthread_mutex_lock(&session_list_mutex);
    list_for_each_entry(p, &session_list, list) {
        if (!strcmp(p->session_id, session_id)) {
            se = p;
            if (se->status == SESSION_IN_FREE)
                se = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&session_list_mutex);
    return se;
}

struct session *session_create(struct Uri *uri, struct bufferevent *bev, int client_rtp_port, int client_rtcp_port)
{
    struct session *se = NULL;
    struct timeval tv;
    struct sockaddr clit_addr;
    struct sockaddr_in *clit_addr_in;
    socklen_t addr_len = sizeof(struct sockaddr);
    int port, ret;
    char clit_ip[16];

    if (!uri || !bev || client_rtp_port < 1025 || client_rtcp_port < 1025) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }

    ret = getpeername(bev->ev_read.ev_fd, &clit_addr, &addr_len);
    if (ret != 0) {
        printf("%s: getpername failed: %s\n", __func__, strerror(errno));
        return NULL;
    }

    /*
    if (clit_addr.sa_family != AF_INET) {
        printf("%s: Only serves on IPv4\n", __func__);
        release_uri(uri);
        return NULL;
    }
    */

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
    while (1) {
        if (port > 65534)
            goto bind_failed;

        uv_ip4_addr(active_addr, port++, &se->serv_rtp_addr);
        ret = uv_udp_bind(&se->rtp_handle, (struct sockaddr*)&se->serv_rtp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret) {
            printf("%s: bind RTP addr failed: %s\n", __func__, uv_strerror(ret));
            goto bind_failed;
        }

        uv_ip4_addr(active_addr, port++, &se->serv_rtcp_addr);
        ret = uv_udp_bind(&se->rtcp_handle, (struct sockaddr*)&se->serv_rtcp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret) {
            printf("%s: bind RTCP addr failed: %s\n", __func__, uv_strerror(ret));
            goto bind_failed;
        }

        break;
    }

    gettimeofday(&tv, NULL);
    // XXX: need more complex ?
    snprintf(se->session_id, 9, "%04x%04x", (unsigned int)tv.tv_usec, (unsigned int)rand());

    ret = ref_uri(se);
    if (ret < 0) {
        printf("%s: Cannot reference uri(%s)\n", __func__, se->uri->url);
        goto ref_uri_failed;
    }

    se->status = SESSION_IDLE;

    pthread_mutex_lock(&session_list_mutex);
    list_add(&se->list, &session_list);
    pthread_mutex_unlock(&session_list_mutex);

    return se;

ref_uri_failed:
    ;
bind_failed:
    uv_close((uv_handle_t*)&se->rtp_handle, rtp_handle_close_cb);
    se->rtp_handle_status = HANDLE_CLOSING;
init_rtp_handle_failed:
    uv_close((uv_handle_t*)&se->rtcp_handle, rtcp_handle_close_cb);
    se->rtcp_handle_status = HANDLE_CLOSING;
init_rtcp_handle_failed:
    session_destroy(se);
    return NULL;
}

void session_list_init()
{
    INIT_LIST_HEAD(&session_list);
}

