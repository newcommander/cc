#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ifaddrs.h>

#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <uv.h>

#include "encoder.h"
#include "rtsp.h"

#define CC_IDLE    0
#define CC_IN_INIT 1
#define CC_IN_FREE 2
static int cc_status;

static const int PORT = 554;
char active_addr[128];
char base_url[1024];

static pthread_t rtcp_thread = 0;
static pthread_t rtp_thread = 0;

static void read_cb(struct bufferevent *bev, void *user_data)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    struct rtsp_request *rr = NULL;
    struct session *se = NULL;
    char *response_str = NULL, *p = NULL;
    int ret = 0, offset = 0;

    size_t len = evbuffer_get_length(input);
    char *buf = (char*)calloc(len + 1, 1);
    if (!buf) {
        printf("%s: calloc failed\n", __func__);
        make_error_reply(500, 0, &response_str);
        goto reply;
    }

    p = buf;
    offset = 0;
    while ((offset = bufferevent_read(bev, p, buf + len - p)) > 0)
        p += offset;

    ret = convert_rtsp_request(&rr, bev, buf, len);
    if (ret != 0) {
        make_error_reply(ret, rr ? rr->rh.cseq : 0, &response_str);
        goto reply;
    }

    ret = make_response(rr, &response_str);
    if (ret != 0) {
        if (response_str) {
            free(response_str);
            response_str = NULL;
        }
        make_error_reply(ret, rr->rh.cseq, &response_str);
    }

reply:
    if (response_str) {
        bufferevent_write(bev, response_str, strlen(response_str));
        bufferevent_flush(bev, EV_WRITE, BEV_FLUSH);
        free(response_str);
    }

    if (ret != 0) {
        se = (struct session*)bev->wm_read.private_data;
        if (se)
            session_destroy(se);
        else
            bufferevent_free(bev);
    }

    if (rr)
        release_rtsp_request(rr);
    if (buf)
        free(buf);
}

static void write_cb(struct bufferevent *bev, void *user_data)
{
    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0) {
        ;
    }
}

static void event_cb(struct bufferevent *bev, short events, void *user_data)
{
    struct session *se = NULL;

    if (events & BEV_EVENT_EOF) {
        printf("%s: Connection closed.\n", __func__);
    } else if (events & BEV_EVENT_ERROR) {
        printf("%s: Got an error on the connection: %s\n", __func__, strerror(errno));
    } else {
        switch (events) {
        case BEV_EVENT_READING:
            printf("%s: Unexpected event: BEV_EVENT_READING\n", __func__);
            break;
        case BEV_EVENT_WRITING:
            printf("%s: Unexpected event: BEV_EVENT_WRITING\n", __func__);
            break;
        case BEV_EVENT_TIMEOUT:
            printf("%s: Unexpected event: BEV_EVENT_TIMEOUT\n", __func__);
            break;
        case BEV_EVENT_CONNECTED:
            printf("%s: Unexpected event: BEV_EVENT_CONNECTED\n", __func__);
            break;
        default:
            printf("%s: Unknow event: 0x%x\n", __func__, events);
        }
        return;
    }

    bufferevent_flush(bev, EV_WRITE, BEV_FLUSH);
    se = (struct session*)bev->wm_read.private_data;
    if (se)
        session_destroy(se);
    else
        bufferevent_free(bev);
}

static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *user_data)
{
    struct event_base *base = (struct event_base*)user_data;
    struct bufferevent *bev = NULL;

    if (cc_status != CC_IDLE)
        return;

    if (!base) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }

    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        printf("%s: Error constructing bufferevent!", __func__);
        event_base_loopbreak(base);
        return;
    }
    bev->wm_read.private_data = NULL;

    bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);
    bufferevent_enable(bev, EV_WRITE);
    bufferevent_enable(bev, EV_READ);
}

static void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
    struct event_base *base = (struct event_base*)user_data;
    struct timeval delay = { 0, 500000 }; // 0.5 second

    if (!base) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }
    cc_status = CC_IN_FREE;
    event_base_loopexit(base, &delay);
}

static int get_active_address(char *nic_name, char *buf, int len)
{
    struct sockaddr_in *sin = NULL;
    struct ifaddrs *ifa = NULL, *iflist = NULL;
    char *tmp = NULL;

    if (!buf || len <= 0) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    if (!nic_name) {
        printf("set active NIC to default: eth0\n");
        nic_name = "eth0";
    }

    if (getifaddrs(&iflist) < 0) {
        printf("%s: getting NIC[%s] addresses failed: %s\n", __func__, nic_name, strerror(errno));
        return -1;
    }

    for (ifa = iflist; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (!strcmp(nic_name, ifa->ifa_name)) {
                sin = (struct sockaddr_in*)ifa->ifa_addr;
                tmp = inet_ntoa(sin->sin_addr);
                strncpy(buf, tmp, len < strlen(tmp) ? len : strlen(tmp));
            }
        }
    }

    if (!tmp) {
        printf("%s: Could not find address for NIC[%s]\n", __func__, nic_name);
        freeifaddrs(iflist);
        return -1;
    }

    freeifaddrs(iflist);
    return 0;
}

void* rtcp_dispatch(void *arg);
void* rtp_dispatch(void *arg);
void *cc_stream(void *arg)
{
    struct event_base *base = NULL;
    struct evconnlistener *listener = NULL;
    struct event *signal_event = NULL;
    struct sockaddr_in sin;
    char *nic_name = (char*)arg;

    cc_status = CC_IN_INIT;

    memset(active_addr, 0, sizeof(active_addr));
    if (get_active_address(nic_name, active_addr, sizeof(active_addr)) < 0)
        return NULL;

    memset(base_url, 0, sizeof(base_url));
    strncat(base_url, "rtsp://", 7);
    strncat(base_url, active_addr, strlen(active_addr));

    register_encoders();
    uris_init(base_url);
    session_list_init();

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        goto event_base_new_failed;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(active_addr);
    sin.sin_port = htons(PORT);

    listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
            LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)&sin, sizeof(sin));
    if (!listener) {
        fprintf(stderr, "Could not create a listener!\n");
        goto listener_bind_failed;
    }

    signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
    if (!signal_event || event_add(signal_event, NULL) < 0) {
        fprintf(stderr, "Could not create/add a signal event!\n");
        goto evsignal_new_failed;
    }

    cc_status = CC_IDLE;

    if (pthread_create(&rtcp_thread, NULL, rtcp_dispatch, NULL) != 0) {
        printf("%s: Creating rtcp thread failed: %s\n", __func__, strerror(errno));
        rtcp_thread = 0;
        goto create_rtcp_thread_failed;
    }
    if (pthread_create(&rtp_thread, NULL, rtp_dispatch, NULL) != 0) {
        printf("%s: Creating rtp thread failed: %s\n", __func__, strerror(errno));
        rtp_thread = 0;
        goto create_rtp_thread_failed;
    }

    event_base_dispatch(base);

    cc_status = CC_IN_FREE;

    rtp_stop();
create_rtp_thread_failed:
    rtcp_stop();
create_rtcp_thread_failed:
    evconnlistener_free(listener);
evsignal_new_failed:
    event_free(signal_event);
listener_bind_failed:
    event_base_free(base);
event_base_new_failed:
    session_destroy_all();
    uris_deinit();

    return NULL;
}
