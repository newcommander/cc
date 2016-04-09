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
#include <time.h>

#include "rtsp.h"

static const char MESSAGE[] = "Hello, World!\n";

static const int PORT = 554;

static char* find_phrase_by_code(char *code)
{
    status_code *p = NULL;
    if (!code) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }
    p = response_code;
    while (p && p->code) {
        if (!strcmp(code, p->code))
            return p->phrase;
        p++;
    }
    printf("%s: no elem found\n", __func__);
    return NULL;
}

static char* find_code_by_phrase(char *phrase)
{
    status_code *p = NULL;
    if (!phrase) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }
    p = response_code;
    while (p && p->phrase) {
        if (!strcmp(phrase, p->phrase))
            return p->code;
        p++;
    }
    printf("%s: no elem found\n", __func__);
    return NULL;
}

static int make_status_line(char **buf, char *code, char *phrase)
{
    if (!code && !phrase) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    else if (code && phrase) {
        *buf = (char*)calloc(13 + strlen(code) + strlen(phrase), 1);
        if (!*buf)
            goto calloc_failed;
        sprintf(*buf, "RTSP/1.0 %s %s\r\n", code, phrase);
        return 0;
    } else if (code && !phrase) {
        phrase = find_phrase_by_code(code);
        if (!phrase)
            return -1;
        *buf = (char*)calloc(13 + strlen(code) + strlen(phrase), 1);
        if (!*buf)
            goto calloc_failed;
        sprintf(*buf, "RTSP/1.0 %s %s\r\n", code, phrase);
        return 0;
    } else {
        code = find_code_by_phrase(phrase);
        if (!code)
            return -1;
        *buf = (char*)calloc(13 + strlen(code) + strlen(phrase), 1);
        if (!*buf)
            goto calloc_failed;
        sprintf(*buf, "RTSP/1.0 %s %s\r\n", code, phrase);
        return 0;
    }

calloc_failed:
    printf("%s: calloc failed\n", __func__);
    return -1;
}

static int make_response_cseq(char **cseq_str, int cseq)
{
    *cseq_str = (char*)calloc(32, 1);
    if (!(*cseq_str)) {
        printf("%s: calloc failed\n", __func__);
        return -1;
    }
    sprintf(*cseq_str, "CSeq: %d\r\n", cseq);
    return 0;
}

static int make_response_date(char **time_str)
{
    char *wday[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    char *mon[] = {"Jan","Feb","Mar","Apr","May","June","July","Aug","Sept","Oct","Nov","Dec"};
    struct tm tt;
    time_t t;

    *time_str = (char*)calloc(40, 1);
    if (!(*time_str)) {
        printf("%s: calloc failed\n", __func__);
        return -1;
    }
    t = time(NULL);
    gmtime_r(&t, &tt);
    sprintf(*time_str, "Date: %s, %s %d %d %d:%d:%d GMT\r\n", wday[tt.tm_wday], mon[tt.tm_mon],
            tt.tm_mday, 1900 + tt.tm_year, (tt.tm_hour + 8) % 24, tt.tm_min, tt.tm_sec);

    return 0;
}

static int make_response_for_options(char **response, int cseq)
{
    char *public = "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER";
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL;
    int ret, len = 0;

    ret = make_status_line(&status_line, "200", NULL);
    if (!status_line)
        return -1;
    ret = make_response_cseq(&cseq_str, cseq);
    if (!cseq_str) {
        free(status_line);
        return -1;
    }
    ret = make_response_date(&date_str);
    if (!cseq_str) {
        free(status_line);
        free(cseq_str);
        return -1;
    }
    *response = (char*)calloc(strlen(status_line) + strlen(cseq_str) + strlen(date_str) + strlen(public) + 13, 1);
    if (!(*response)) {
        printf("%s: calloc failed\n", __func__);
        free(status_line);
        free(cseq_str);
        free(date_str);
        return -1;
    }
    snprintf(*response, strlen(status_line) + 1, status_line);
    strncat(*response, cseq_str, strlen(cseq_str));
    strncat(*response, date_str, strlen(date_str));
    snprintf((*response) + strlen(*response), strlen(public) + 11, "Public: %s\r\n", public);
    strncat(*response, "\r\n", 2);

    return 0;
}

static int make_response(rtsp_request *rr, char **buf)
{
    if (!rr) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    switch (rr->method) {
    case MTH_OPTIONS:
        make_response_for_options(buf, rr->rh.cseq);
        break;
    default:
        printf("Unknow RTSP request method\n");
        return -1;
    }
    return 0;
}

static int get_request_line(rtsp_request *rr, char *buf, int len)
{
    char *p_head = NULL, *p_tail = NULL;

    if (!rr || !buf) {
        printf("Invalid parameter\n");
        return -1;
    }

    for (p_head = buf, p_tail = buf; *p_tail != ' ' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != ' ') {
        printf("Invalid RTSP request when get method\n");
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
        printf("Invalid RTSP request when get uri\n");
        return -1;
    }

    if (strncmp("rtsp://", p_head, 7)) {
        printf("Invalid protocol: %s\n", p_head);
        return -1;
    }
    rr->uri = (char*)calloc(p_tail - p_head, 1);
    if (!rr->uri) {
        printf("calloc for uri failed\n");
        return -1;
    }
    strncpy(rr->uri, p_head, p_tail - p_head);

    for (p_head = ++p_tail; *p_tail != '\r' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != '\r') {
        printf("Invalid RTSP request when get version\n");
        free(rr->uri);
        return -1;
    }

    if (p_tail - p_head > 32) {
        printf("version string too long\n");
        return -1;
    }
    strncpy(rr->version, p_head, p_tail - p_head);

    return 0;
}

static int get_request_header(rtsp_request *rr, char *buf, int len)
{
    char *p_head = NULL, *p_tail = NULL;
    int ret = 0;
    char tmp_buf[128], *tmp = NULL;
    memset(tmp_buf, 0, 128);

    if (!buf) {
        printf("Invalid parameter\n");
        return -1;
    }

    // skip request line
    p_tail = strstr(buf, "\r\n");
    if (!p_tail) {
        printf("Invalid RTSP format");
        return -1;
    }

    p_tail += 2;
    p_head = p_tail;
    while (p_tail <= buf + len - 1) {
        p_tail = strstr(p_tail, "\r\n");
        if (!p_tail)
            break;

        tmp = strstr(p_head, ":");
        if (tmp > p_tail) {
            p_tail += 2;
            p_head = p_tail;
            continue;
        }

        if (!strncmp("CSeq", p_head, tmp - p_head)) {
            while (*(++tmp) == ' ') ;
            if (tmp == p_tail) {
                p_tail += 2;
                p_head = p_tail;
                continue;
            }
            strncpy(tmp_buf, tmp, p_tail - tmp);
            rr->rh.cseq = atoi(tmp_buf);
        } else if (!strncmp("Accept", p_head, tmp - p_head)) {
            while (*(++tmp) == ' ') ;
            if (tmp == p_tail) {
                p_tail += 2;
                p_head = p_tail;
                continue;
            }
            rr->rh.accept = (char*)calloc(p_tail - tmp + 1, 1);
            if (rr->rh.accept)
                strncpy(rr->rh.accept, tmp, p_tail - tmp);
        } else if (!strncmp("User-Agent", p_head, tmp - p_head)) {
            while (*(++tmp) == ' ') ;
            if (tmp == p_tail) {
                p_tail += 2;
                p_head = p_tail;
                continue;
            }
            rr->rh.user_agent = (char*)calloc(p_tail - tmp + 1, 1);
            if (rr->rh.user_agent)
                strncpy(rr->rh.user_agent, tmp, p_tail - tmp);
        } else {
            //strncpy(tmp_buf, p_head, tmp - p_head < 127 ? tmp - p_head : 127);
            //printf("Unknow header: %s\n", tmp_buf);
        }

        p_tail += 2;
        p_head = p_tail;
    }

    return 0;
}

static rtsp_request* convert_rtsp_request(char* buf, int len)
{
    int ret = 0;
    rtsp_request* rr = NULL;

    if (!buf) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }
    rr = (rtsp_request*)calloc(1, sizeof(rtsp_request));
    if (!rr) {
        printf("%s: calloc RTSP request failed\n", __func__);
        return NULL;
    }

    ret = get_request_line(rr, buf, len);
    if (ret != 0) {
        free(rr);
        return NULL;
    }

    ret = get_request_header(rr, buf, len);
    if (ret != 0) {
        free(rr);
        return NULL;
    }

    return rr;
}

static void cc_read_cb(struct bufferevent *bev, void *user_data)
{
    rtsp_request *rr = NULL;
    char *response_str = NULL;
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

    rr = convert_rtsp_request(buf, len);
    if (!rr)
        return;

    make_response(rr, &response_str);
    if (!response_str) {
        printf("%s: Could not make response content for request\n", __func__);
        free(rr);
        return;
    }
    free(rr);

    bufferevent_write(bev, response_str, strlen(response_str));
    free(response_str);
}

static void cc_write_cb(struct bufferevent *bev, void *user_data)
{
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		printf("flushed answer\n");
		//bufferevent_free(bev);
	}
}

static void cc_event_cb(struct bufferevent *bev, short events, void *user_data)
{
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
