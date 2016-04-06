/*
  This exmple program provides a trivial server program that listens for TCP
  connections on port 9995.  When they arrive, it writes a short message to
  each client connection, and closes each connection once it is flushed.

  Where possible, it exits cleanly in response to a SIGINT (ctrl-c).
*/

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include "rtsp.h"

static const char MESSAGE[] = "Hello, World!\n";

static const int PORT = 554;

static int get_request_line(Rtsp_Request *rr, const char *buf, int len)
{
    char *p_head = NULL, *p_tail = NULL;

    if (!rr || !buf) {
        printf("Invalide parameter\n");
        return -1;
    }

    for (p_head = buf, p_tail = buf; *p_tail != ' ' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != ' ') {
        printf("Invalide RTSP request when get method\n");
        return -1;
    }

    if (!strncmp("OPTIONS", p_head, p_tail - p_head)) {
        rr->method = MTH_OPTIONS;
    } else if (!strncmp("DESCRIBE", p_head, p_tail - p_head)) {
        rr->method = MTH_DESCRIBE;
    } else if (!strncmp("SETUP", p_head, p_tail - p_head)) {
        rr->method = MTH_SETUP;
    } else if (!strncmp("PLAY", p_head, p_tail - p_head)) {
        rr->method = MTH_PLAY;
    } else if (!strncmp("PAUSE", p_head, p_tail - p_head)) {
        rr->method = MTH_PAUSE;
    } else if (!strncmp("TEARDOWN", p_head, p_tail - p_head)) {
        rr->method = MTH_TEARDOWN;
    } else if (!strncmp("RECORD", p_head, p_tail - p_head)) {
        rr->method = MTH_RECORD;
    } else if (!strncmp("ANNOUNCE", p_head, p_tail - p_head)) {
        rr->method = MTH_ANNOUNCE;
    } else if (!strncmp("GET_PARAMETER", p_head, p_tail - p_head)) {
        rr->method = MTH_GET_PARAMETER;
    } else if (!strncmp("REDIRECT", p_head, p_tail - p_head)) {
        rr->method = MTH_REDIRECT;
    } else if (!strncmp("SET_PARAMETER", p_head, p_tail - p_head)) {
        rr->method = MTH_SET_PARAMETER;
    }

    for (p_head = ++p_tail; *p_tail != ' ' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != ' ') {
        printf("Invalide RTSP request when get url\n");
        return -1;
    }

    if (strncmp("rtsp://", p_head, 7)) {
        printf("Invalide protocol: %s\n", p_head);
        return -1;
    }
    rr->url = (char*)calloc(p_tail - p_head, 1);
    if (!rr->url) {
        printf("calloc for url failed\n");
        return -1;
    }
    strncpy(rr->url, p_head, p_tail - p_head);

    for (p_head = ++p_tail; *p_tail != ' ' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != ' ') {
        printf("Invalide RTSP request when get version\n");
        free(rr->url);
        return -1;
    }

    if (p_tail - p_head > 32) {
        printf("version string too long\n");
        return -1;
    }
    strncpy(rr->version, p_head, p_tail - p_head);

    return 0;
}

static int get_rtsp_request(const char* buf, int len)
{
    int ret = 0;
    Rtsp_Request rr;
    memset(&rr, 0, sizeof(Rtsp_Request));
    rr.method = -1;

    ret = get_request_line(&rr, buf, len);
    if (ret != 0) {
        printf("get request line failed\n");
        return -1;
    }
    printf("method: %d\nurl: %s\nversion: %s\n", rr.method, rr.url, rr.version);

//    ret = get_request_header(&rr, buf, len);
    if (ret != 0) {
        printf("get request header failed\n");
        return -1;
    }

    return ret;
}

static void cc_read_cb(struct bufferevent *bev, void *user_data)
{
    printf("read\n");
	struct evbuffer *input = bufferevent_get_input(bev);

    size_t len = evbuffer_get_length(input);
    char *buf = (char*)calloc(len + 1, 1);
    if (!buf) {
        printf("calloc failed\n");
        return;
    }

    char *p = buf;
    int offset = 0;
    while ((offset = bufferevent_read(bev, p, sizeof(buf))) > 0)
        p += offset;

    printf("[%d]\n%s\n", len, buf);
    get_rtsp_request(buf, len);
}

static void cc_write_cb(struct bufferevent *bev, void *user_data)
{
    printf("write\n");
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		printf("flushed answer\n");
		//bufferevent_free(bev);
	}
}

static void cc_event_cb(struct bufferevent *bev, short events, void *user_data)
{
    printf("event\n");
	if (events & BEV_EVENT_EOF) {
		printf("Connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",
		    strerror(errno));
	}
	/* None of the other events can happen here, since we haven't enabled
	 * timeouts */
	bufferevent_free(bev);
}

static void cc_listener_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *user_data)
{
	struct event_base *base = user_data;
	struct bufferevent *bev;

    printf("listener\n");
	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}

    bufferevent_setcb(bev, cc_read_cb, cc_write_cb, cc_event_cb, NULL);
	bufferevent_enable(bev, EV_WRITE);
	bufferevent_enable(bev, EV_READ);

	//bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

static void cc_signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	struct event_base *base = user_data;
	struct timeval delay = { 2, 0 };

    printf("signal\n");
	printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");

	event_base_loopexit(base, &delay);
}

int main(int argc, char **argv)
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct event *signal_event;

	struct sockaddr_in sin;

	base = event_base_new();
	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);

	listener = evconnlistener_new_bind(base, cc_listener_cb, (void *)base, LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)&sin, sizeof(sin));
	if (!listener) {
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}

	signal_event = evsignal_new(base, SIGINT, cc_signal_cb, (void *)base);
	if (!signal_event || event_add(signal_event, NULL)<0) {
		fprintf(stderr, "Could not create/add a signal event!\n");
		return 1;
	}

	event_base_dispatch(base);

	evconnlistener_free(listener);
	event_free(signal_event);
	event_base_free(base);

	printf("done\n");
	return 0;
}
