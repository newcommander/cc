// RTSP(rfc2326) https://tools.ietf.org/html/rfc2326

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>

#include "rtsp.h"

#define msleep(x) \
    do { \
        struct timespec time_to_wait; \
        time_to_wait.tv_sec = 0; \
        time_to_wait.tv_nsec = 1000 * 1000 * x; \
        nanosleep(&time_to_wait, NULL); \
    } while(0)

static const char MESSAGE[] = "Hello, World!\n";
static char active_addr[16];
static const int PORT = 554;

struct cc_list_head uri_list;
struct cc_list_head session_list;

static rtsp_uri *g_uri = NULL;

rtsp_uri* alloc_rtsp_uri(char *url)
{
    rtsp_uri *uri = NULL;
    if (!url) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }

    uri = (rtsp_uri*)calloc(1, sizeof(rtsp_uri));
    if (!uri) {
        printf("%s: calloc failed for rtsp_uri\n", __func__);
        return NULL;
    }
    pthread_mutex_init(&uri->ref_mutex, NULL);

    uri->url = (char*)calloc(strlen(url) + 1, 1);
    if (!uri->url) {
        printf("%s: calloc failed for url of uri\n", __func__);
        free(uri);
        return NULL;
    }
    strncpy(uri->url, url, strlen(url));

    cc_list_add_tail(&uri->list, &uri_list);
    return uri;
}

void release_rtsp_uri(rtsp_uri *uri)
{
    int ref = 0;
    if (!uri)
        return;
    pthread_mutex_lock(&uri->ref_mutex);
    ref = uri->ref;
    pthread_mutex_unlock(&uri->ref_mutex);

    if (ref > 0)
        return;

    cc_list_del(&uri->list);
    free(uri->url);
    free(uri);
}

rtsp_uri* find_rtsp_uri(char *url)
{
    rtsp_uri *uri = NULL, *p = NULL;
    if (!url)
        return NULL;
    cc_list_for_each_entry (p, &uri_list, list) {
        if (!strcmp(p->url, url)) {
            uri = p;
            break;
        }
    }
    return uri;
}

void release_rtsp_request(rtsp_request *rr)
{
    if (!rr)
        return;
	if (rr->url)
        free(rr->url);
	if (rr->rh.accept)
        free(rr->rh.accept);
	if (rr->rh.user_agent)
        free(rr->rh.user_agent);
	if (rr->rh.transport)
        free(rr->rh.transport);
    if (rr->rh.range)
        free(rr->rh.range);
}

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

static int make_response_for_options(int cseq, char **response)
{
    char *public = "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN";
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL;
    int ret;

    ret = make_status_line(&status_line, "200", NULL);
    if (!status_line) {
		ret = 500;
		goto out;
	}
    ret = make_response_cseq(&cseq_str, cseq);
    if (!cseq_str) {
        ret = 500;
        goto out;
    }
    ret = make_response_date(&date_str);
    if (!cseq_str) {
		ret = 500;
		goto out;
    }
    *response = (char*)calloc(strlen(status_line) + strlen(cseq_str) + strlen(date_str) + strlen(public) + 13, 1);
    if (!(*response)) {
        printf("%s: calloc failed\n", __func__);
		ret = 500;
		goto out;
    }
    snprintf(*response, strlen(status_line) + 1, status_line);
    strncat(*response, cseq_str, strlen(cseq_str));
    strncat(*response, date_str, strlen(date_str));
    snprintf((*response) + strlen(*response), strlen(public) + 11, "Public: %s\r\n", public);
    strncat(*response, "\r\n", 2);

	ret = 0;

out:
	if (status_line)
		free(status_line);
	if (cseq_str)
		free(cseq_str);
	if (date_str)
		free(date_str);
    return ret;
}

static int make_entity_header(rtsp_uri *uri, char **buf, int sdp_len)
{
    char sdp_len_str[32];
    if (!uri || !buf || (sdp_len < 0)) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    memset(sdp_len_str, 0, 32);
    snprintf(sdp_len_str, 32, "%d", sdp_len);

    *buf = (char*)calloc(strlen(uri->url) + strlen(sdp_len_str) + 66, 1);
    if (!(*buf)) {
        printf("%s: calloc failed\n", __func__);
        return -1;
    }
    snprintf(*buf, strlen(uri->url) + 17, "Content-Base: %s\r\n", uri->url);
    strncat(*buf, "Content-type: application/sdp\r\n", 31);
    snprintf((*buf) + strlen(*buf), strlen(sdp_len_str) + 19, "Content-length: %s\r\n", sdp_len_str);

    return 0;
}

static int make_sdp_string(char **buf)
{
    char *version = "v=0\r\n";
    char origin[1024];
    char *session_name = "s=live from cc\r\n";
    char *time = "t=0 0\r\n";
    char *media_desc = "m=video 0 RTP/AVP 96\r\n";
    char *media_attr = "a=rtpmap:96 H264/90000\r\n";    // how to set this attribute ???
    struct timeval tv;

    if (!buf) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    gettimeofday(&tv, NULL);

    memset(origin, 0, 1024);
    strncat(origin, "o=- ", 6);
    snprintf(origin + strlen(origin), 20, "%ld.%ld ", tv.tv_sec, tv.tv_usec);
    strncat(origin, "1 IN IP4 ", 9);
    strncat(origin, active_addr, strlen(active_addr));
    strncat(origin, "\r\n", 2);

    *buf = (char*)calloc(strlen(version) + strlen(origin) + strlen(session_name) + \
            strlen(time) + strlen(media_desc) + strlen(media_attr), 1);
    if (!(*buf)) {
        printf("%s: calloc failed\n", __func__);
        return -1;
    }
    strncat(*buf, version, strlen(version));
    strncat(*buf, origin, strlen(origin));
    strncat(*buf, session_name, strlen(session_name));
    strncat(*buf, time, strlen(time));
    strncat(*buf, media_desc, strlen(media_desc));
    strncat(*buf, media_attr, strlen(media_attr));

    return 0;
}

static void release_rtsp_session(rtsp_session *rs);
static rtsp_session* create_rtsp_session(rtsp_request *rr)
{
    rtsp_session *rs = NULL;
    rtsp_uri *uri = NULL;
    struct timeval tv;
    int port, ret;

    if (!rr || !rr->bev) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }

    uri = find_rtsp_uri(rr->url);
    if (!uri) {
        pthread_mutex_unlock(&uri->ref_mutex);
        printf("%s: No uri '%s' found", __func__, rr->url);
        return NULL;
    }
    pthread_mutex_lock(&uri->ref_mutex);
    uri->ref++;
    pthread_mutex_unlock(&uri->ref_mutex);

    rs = (rtsp_session*)calloc(1, sizeof(rtsp_session)); // TODO : alloc, init, add to list
    if (!rs) {
        printf("%s: calloc failed\n", __func__);
        return NULL;
    }
    rs->uri = uri;
    rs->bev = rr->bev;

    if (ret = uv_loop_init(&rs->rtp_loop)) {
        printf("%s: init RTP UDP loop failed: %s\n", __func__, uv_strerror(ret));
        goto failed_rtp_loop_init;
    }
    if (ret = uv_loop_init(&rs->rtcp_loop)) {
        printf("%s: init RTCP UDP loop failed: %s\n", __func__, uv_strerror(ret));
        goto failed_rtcp_loop_init;
    }
    if (ret = uv_udp_init(&rs->rtp_loop, &rs->rtp_handle)) {
        printf("%s: init RTP UDP handle failed: %s\n", __func__, uv_strerror(ret));
        goto failed;
    }
    if (ret = uv_udp_init(&rs->rtcp_loop, &rs->rtcp_handle)) {
        printf("%s: init RTCP UDP handle failed: %s\n", __func__, uv_strerror(ret));
        goto failed;
    }

    port = 1025;
    ret = 1;
    while (ret) {
        uv_ip4_addr(active_addr, port++, &rs->rtp_addr);
        ret = uv_udp_bind(&rs->rtp_handle, (struct sockaddr*)&rs->rtp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret)
            goto failed;
        uv_ip4_addr(active_addr, port++, &rs->rtcp_addr);
        ret = uv_udp_bind(&rs->rtcp_handle, (struct sockaddr*)&rs->rtcp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret)
            goto failed;

        if (port > 65534)
            goto failed;

        break;
    }
    gettimeofday(&tv, NULL);
    snprintf(rs->session_id, 9, "%04x%04x", tv.tv_usec, rand());

    cc_list_add_tail(&rs->list, &session_list);

    return rs;

failed:
    pthread_mutex_lock(&uri->ref_mutex);
    uri->ref--;
    pthread_mutex_unlock(&uri->ref_mutex);
    uv_loop_close(&rs->rtcp_loop);
failed_rtcp_loop_init:
    uv_loop_close(&rs->rtp_loop);
failed_rtp_loop_init:
    release_rtsp_session(rs);
    return NULL;
}

static void release_rtsp_session(rtsp_session *rs)
{
    struct evbuffer *output = NULL;
    int count = 0;

    if (!rs)
        return;

    cc_list_del(&rs->list);

    output = bufferevent_get_output(rs->bev);
    while (evbuffer_get_length(output)) {
        msleep(100);
        if (count > 10)
            break;
    }
    bufferevent_free(rs->bev);

    uv_udp_recv_stop(&rs->rtp_handle);
    uv_udp_recv_stop(&rs->rtcp_handle);

    if (uv_loop_alive(&rs->rtcp_loop))
        uv_stop(&rs->rtcp_loop);
    if (uv_loop_alive(&rs->rtp_loop))
        uv_stop(&rs->rtp_loop);

    pthread_mutex_lock(&rs->uri->ref_mutex);
    rs->uri->ref--;
    pthread_mutex_unlock(&rs->uri->ref_mutex);

    free(rs);
}

static rtsp_session* find_rtsp_session_by_id(char *session_id)
{
    rtsp_session *rs = NULL, *p = NULL;
    if (!session_id)
        return NULL;
    cc_list_for_each_entry (p, &session_list, list) {
        if (!strcmp(p->session_id, session_id)) {
            rs = p;
            break;
        }
    }
    return rs;
}

static int make_response_for_describe(rtsp_request *rr, char **response)
{
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL, *entity_header = NULL, *sdp_str = NULL;
    int len = 0;
	rtsp_uri *uri = NULL;

    if (!response || !rr || !rr->rh.accept) {
        printf("%s: Invalid parameter\n", __func__);
        *response = NULL;
        return 500;
    }

    if (!strstr(rr->rh.accept, "application/sdp")) {
        printf("%s: Only accept application/sdp\n", __func__);
        *response = NULL;
        return 406;
    }

    uri = find_rtsp_uri(rr->url);
    if (!uri) {
        printf("%s: No uri '%s' found", __func__, rr->url);
        return 404;
    }

	make_status_line(&status_line, "200", NULL);
    if (!status_line)
        goto out;
    make_response_cseq(&cseq_str, rr->rh.cseq);
    if (!cseq_str)
        goto out;
    make_response_date(&date_str);
    if (!cseq_str)
        goto out;
    make_sdp_string(&sdp_str);
    if (!sdp_str)
        goto out;
    make_entity_header(uri, &entity_header, strlen(sdp_str));
    if (!entity_header)
        goto out;

    len = strlen(status_line) + strlen(cseq_str) + strlen(date_str) + strlen(entity_header) + 2 + strlen(sdp_str);
    *response = (char*)calloc(len + 1, 1);
    if (!(*response)) {
        printf("%s: calloc failed\n", __func__);
        goto out;
    }
    strncat(*response, status_line, strlen(status_line));
    strncat(*response, cseq_str, strlen(cseq_str));
    strncat(*response, date_str, strlen(date_str));
    strncat(*response, entity_header, strlen(entity_header));
    strncat(*response, "\r\n", 2);
    strncat(*response, sdp_str, strlen(sdp_str));

    free(status_line);
    free(cseq_str);
    free(date_str);
    free(entity_header);
    free(sdp_str);
    return 0;

out:
    if (status_line)
        free(status_line);
    if (cseq_str)
        free(cseq_str);
    if (date_str)
        free(date_str);
    if (entity_header)
        free(entity_header);
    if (sdp_str)
        free(sdp_str);
    return 500;
}

static int make_response_for_setup(rtsp_request *rr, char **response)
{
    rtsp_session *rs = NULL;
	char *p_head = NULL, *p_tail = NULL, *p_tmp = NULL;
	char *protocol = NULL, *mode = NULL;
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL;
	int c_rtp_port = 0, c_rtcp_port = 0;
	int len;

	if (!rr || !rr->rh.transport) {
		printf("%s: Could not find request header 'Transport'\n", __func__);
		return 400;
	}
	len = strlen(rr->rh.transport);

	p_head = rr->rh.transport;
	p_tail = rr->rh.transport;
	while (p_tail < rr->rh.transport + len) {
		p_tail = strstr(p_head, ";");
		if (!p_tail)
			p_tail = rr->rh.transport + len;
		if (!strncmp(p_head, "RTP/AVP/UDP", p_tail - p_head))
			protocol = "RTP/AVP/UDP";
		else if (!strncmp(p_head, "unicast", p_tail - p_head))
			mode = "unicast";
		else if (p_tmp = strstr(p_head, "=")) {
			if (!strncmp(p_head, "client_port", p_tmp - p_head)) {
				p_head = ++p_tmp;
				if (p_tmp = strstr(p_head, "-")) {
					char port[10];
					memset(port, 0, 10);
					memcpy(port, p_head, 10 < p_tmp - p_head ? 10 : p_tmp - p_head);
					c_rtp_port = atoi(port);
					p_head = ++p_tmp;
					memset(port, 0, 10);
					memcpy(port, p_head, 10 < p_tail - p_head ? 10 : p_tail - p_head);
					c_rtcp_port = atoi(port);
				} else {
					printf("%s: bad client port in RTSP request\n", __func__);
					return 400;
				}
			}
		}
        p_head = p_tail + 1;
	}
	if (!protocol || !mode || !c_rtp_port || !c_rtcp_port) {
		printf("%s: bad RTSP request\n", __func__);
		return 400;
	}

    rs = create_rtsp_session(rr);
    if (!rs)
        return 500;

	make_status_line(&status_line, "200", NULL);
    if (!status_line)
        goto failed;
    make_response_cseq(&cseq_str, rr->rh.cseq);
    if (!cseq_str)
        goto failed;
    make_response_date(&date_str);
    if (!cseq_str)
        goto failed;

    len = strlen(status_line) + strlen(cseq_str) + strlen(date_str) + 120;
    *response = (char*)calloc(len, 1);
    if (!(*response)) {
        printf("%s: calloc failed\n", __func__);
        goto failed;
    }
    strncat(*response, status_line, strlen(status_line));
    strncat(*response, cseq_str, strlen(cseq_str));
    strncat(*response, date_str, strlen(date_str));
    snprintf(*response + strlen(*response), strlen(rs->session_id) + 23, "Session: %s;timeout=60\r\n", rs->session_id);
    snprintf(*response + strlen(*response), len - strlen(*response),
            "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n",
            c_rtp_port, c_rtcp_port, ntohs(rs->rtp_addr.sin_port), ntohs(rs->rtcp_addr.sin_port));
    strncat(*response, "\r\n", 2);

    free(status_line);
    free(cseq_str);
    free(date_str);
	return 0;

failed:
    if (status_line)
        free(status_line);
    if (cseq_str)
        free(cseq_str);
    if (date_str)
        free(date_str);
    release_rtsp_session(rs);
    return 500;
}

static int make_response_for_play(rtsp_request *rr, char **response)
{
    rtsp_session *rs = NULL;
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL;
    int len;

    rs = find_rtsp_session_by_id(rr->rh.session_id);
    if (!rs)
        return 454;

    // play

    make_status_line(&status_line, "200", NULL);
    if (!status_line)
        goto failed;
    make_response_cseq(&cseq_str, rr->rh.cseq);
    if (!cseq_str)
        goto failed;
    make_response_date(&date_str);
    if (!cseq_str)
        goto failed;

    len = strlen(status_line) + strlen(cseq_str) + strlen(date_str) + 120;
    *response = (char*)calloc(len, 1);
    if (!(*response)) {
        printf("%s: calloc failed\n", __func__);
        goto failed;
    }
    strncat(*response, status_line, strlen(status_line));
    strncat(*response, cseq_str, strlen(cseq_str));
    strncat(*response, date_str, strlen(date_str));
    strncat(*response, "\r\n", 2);

    free(status_line);
    free(cseq_str);
    free(date_str);
    return 0;
failed:
    if (status_line)
        free(status_line);
    if (cseq_str)
        free(cseq_str);
    if (date_str)
        free(date_str);
    return 500;
}

static int make_response_for_teardown(rtsp_request *rr, char **response)
{
    rtsp_session *rs = NULL;
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL;
    int len;

    rs = find_rtsp_session_by_id(rr->rh.session_id);
    if (!rs)
        return 454;

    // teardown

    make_status_line(&status_line, "200", NULL);
    if (!status_line)
        goto failed;
    make_response_cseq(&cseq_str, rr->rh.cseq);
    if (!cseq_str)
        goto failed;
    make_response_date(&date_str);
    if (!cseq_str)
        goto failed;

    len = strlen(status_line) + strlen(cseq_str) + strlen(date_str) + 120;
    *response = (char*)calloc(len, 1);
    if (!(*response)) {
        printf("%s: calloc failed\n", __func__);
        goto failed;
    }
    strncat(*response, status_line, strlen(status_line));
    strncat(*response, cseq_str, strlen(cseq_str));
    strncat(*response, date_str, strlen(date_str));
    strncat(*response, "\r\n", 2);

    free(status_line);
    free(cseq_str);
    free(date_str);
    return 0;
failed:
    if (status_line)
        free(status_line);
    if (cseq_str)
        free(cseq_str);
    if (date_str)
        free(date_str);
    return 500;
}

static int make_response(rtsp_request *rr, char **buf)
{
    int ret = 0;
    if (!rr) {
        printf("%s: Invalid parameter\n", __func__);
        return 500;
    }

    // functions make_response_for_xxx need to return response status code
    switch (rr->method) {
    case MTH_OPTIONS:
        ret = make_response_for_options(rr->rh.cseq, buf);
        if (ret != 0)
            printf("%s: Failed making response for OPTIONS\n", __func__);
        break;
    case MTH_DESCRIBE:
        ret = make_response_for_describe(rr, buf);
        if (ret != 0)
            printf("%s: Failed making response for DESCRIBE\n", __func__);
        break;
	case MTH_SETUP:
		ret = make_response_for_setup(rr, buf);
		if (ret != 0)
			printf("%s: Failed making response for SETUP\n", __func__);
		break;
    case MTH_PLAY:
        ret = make_response_for_play(rr, buf);
        if (ret != 0)
            printf("%s: Failed making response for PLAY\n", __func__);
        break;
    case MTH_TEARDOWN:
        ret = make_response_for_teardown(rr, buf);
        if (ret != 0)
            printf("%s: Failed making response for TEARDOWN\n", __func__);
        break;
    default:
        printf("%s: Unknow RTSP request method (code: %d)\n", __func__, rr->method);
        return 405;
    }
    return ret;
}

static int get_request_line(rtsp_request *rr, char *buf, int len)
{
    char *p_head = NULL, *p_tail = NULL;

    if (!rr || !buf) {
        printf("Invalid parameter\n");
        return 500;
    }

    for (p_head = buf, p_tail = buf; *p_tail != ' ' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != ' ') {
        printf("Invalid RTSP request when get method\n");
        return 400;
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
        return 405;
    } else if (!strncmp("ANNOUNCE", p_head, p_tail - p_head)) {
        return 405;
    } else if (!strncmp("GET_PARAMETER", p_head, p_tail - p_head)) {
        rr->method = MTH_GET_PARAMETER;
    } else if (!strncmp("REDIRECT", p_head, p_tail - p_head)) {
        return 405;
    } else if (!strncmp("SET_PARAMETER", p_head, p_tail - p_head)) {
        rr->method = MTH_SET_PARAMETER;
    } else {
        printf("%s: Unknow RTSP request recived: %s\n", __func__, p_head);
        return 400;
    }

    for (p_head = ++p_tail; *p_tail != ' ' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != ' ') {
        printf("Invalid RTSP request when get uri\n");
        return 400;
    }

    if (strncmp("rtsp://", p_head, 7)) {
        printf("Invalid protocol: %s\n", p_head);
        return 400;
    }
    rr->url = (char*)calloc(p_tail - p_head + 1, 1);
    if (!rr->url) {
        printf("calloc for uri failed\n");
        return 500;
    }
    strncpy(rr->url, p_head, p_tail - p_head);

    for (p_head = ++p_tail; *p_tail != '\r' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != '\r') {
        printf("Invalid RTSP request when get version\n");
        free(rr->url);
        return 400;
    }

    if (p_tail - p_head > 32) {
        printf("version string too long\n");
        free(rr->url);
        return 505;
    }
    strncpy(rr->version, p_head, p_tail - p_head);

    return 0;
}

static int get_rtsp_request_header(rtsp_request *rr, char *buf, int len)
{
    char *p_head = NULL, *p_tail = NULL;
    char tmp_buf[128], *tmp = NULL;
    memset(tmp_buf, 0, 128);

    if (!buf) {
        printf("Invalid parameter\n");
        return 500;
    }

    // skip request line
    p_tail = strstr(buf, "\r\n");
    if (!p_tail) {
        printf("Invalid RTSP format");
        return 400;
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
        } else if (!strncmp("Transport", p_head, tmp - p_head)) {
            while (*(++tmp) == ' ') ;
            if (tmp == p_tail) {
                p_tail += 2;
                p_head = p_tail;
                continue;
            }
            rr->rh.transport = (char*)calloc(p_tail - tmp + 1, 1);
            if (rr->rh.transport)
                strncpy(rr->rh.transport, tmp, p_tail - tmp);
        } else if (!strncmp("Session", p_head, tmp - p_head)) {
            while (*(++tmp) == ' ') ;
            if (tmp == p_tail) {
                p_tail += 2;
                p_head = p_tail;
                continue;
            }
            strncpy(rr->rh.session_id, tmp, p_tail - tmp > 32 ? 32 : p_tail - tmp);
        } else if (!strncmp("Range", p_head, tmp - p_head)) {
            while (*(++tmp) == ' ') ;
            if (tmp == p_tail) {
                p_tail += 2;
                p_head = p_tail;
                continue;
            }
            rr->rh.range = (char*)calloc(p_tail - tmp + 1, 1);
            if (rr->rh.range)
                strncpy(rr->rh.range, tmp, p_tail - tmp);
        } else {
            //strncpy(tmp_buf, p_head, tmp - p_head < 127 ? tmp - p_head : 127);
            //printf("Unknow header: %s\n", tmp_buf);
        }

        p_tail += 2;
        p_head = p_tail;
    }

    return 0;
}

static int convert_rtsp_request(rtsp_request **rr, struct bufferevent *bev, char *buf, int len)
{
    int ret = 0;

    if (!buf) {
        printf("%s: Invalid parameter\n", __func__);
        return 500;
    }

    *rr = (rtsp_request*)calloc(1, sizeof(rtsp_request));
    if (!(*rr)) {
        printf("%s: calloc RTSP request failed\n", __func__);
        return 500;
    }
    (*rr)->bev = bev;

    ret = get_request_line(*rr, buf, len);
    if (ret != 0) {
        release_rtsp_request(*rr);
        return ret;
    }

    ret = get_rtsp_request_header(*rr, buf, len);
    if (ret != 0) {
        release_rtsp_request(*rr);
        return ret;
    }

    return 0;
}

void send_error_reply(int code)
{
    if (code <= 0) {
        printf("%s: bad status code: %d\n", __func__, code);
        ; // TODO
    }
    // FIXME : send error reply
}

static void cc_read_cb(struct bufferevent *bev, void *user_data)
{
	struct evbuffer *input = bufferevent_get_input(bev);
    rtsp_request *rr = NULL;
    char *response_str = NULL;
    int ret = 0;

    size_t len = evbuffer_get_length(input);
    char *buf = (char*)calloc(len + 1, 1);
    if (!buf) {
        printf("%s: calloc failed\n", __func__);
        send_error_reply(500);
        return;
    }

    char *p = buf;
    int offset = 0;
    while ((offset = bufferevent_read(bev, p, sizeof(buf))) > 0)
        p += offset;

    ret = convert_rtsp_request(&rr, bev, buf, len);
    free(buf);
    if (!rr) {
        send_error_reply(ret);
        return;
    }

    ret = make_response(rr, &response_str);
    release_rtsp_request(rr);
    if (!response_str) {
        send_error_reply(ret);
        return;
    }

    bufferevent_write(bev, response_str, strlen(response_str));
    //FIXME do free in write callback
    free(response_str);
    return;
}

static void cc_write_cb(struct bufferevent *bev, void *user_data)
{
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
        ;
		//printf("flushed answer\n");
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
	struct timeval delay = { 1, 0 };

    printf("signal\n");
	printf("Caught an interrupt signal; exiting cleanly in 1 second.\n");

	event_base_loopexit(base, &delay);
}

int get_active_address(char *interface, char *buf, int len)
{
    struct sockaddr_in *sin = NULL;
    struct ifaddrs *ifa = NULL, *iflist = NULL;
    char *tmp = NULL;

    if (!buf || len <= 0) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    if (!interface) {
        printf("set active interface to default: eth0\n");
        interface = "eth0";
    }

    if (getifaddrs(&iflist)) {
        printf("%s: getting interface addresses failed: %s\n", __func__, strerror(errno));
        return -1;
    }

    for (ifa = iflist; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (!strcmp(interface, ifa->ifa_name)) {
                sin = (struct sockaddr_in*)ifa->ifa_addr;
                tmp = inet_ntoa(sin->sin_addr);
                strncpy(buf, tmp, len < strlen(tmp) ? len : strlen(tmp));
            }
        }
    }

    if (!tmp) {
        printf("%s: Could not find address for interface %s\n", __func__, interface);
        freeifaddrs(iflist);
        return -1;
    }

    freeifaddrs(iflist);
    return 0;
}

int main(int argc, char **argv)
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct event *signal_event;
    char url[1024];

	struct sockaddr_in sin;

    CC_INIT_LIST_HEAD(&uri_list);
    CC_INIT_LIST_HEAD(&session_list);

    memset(active_addr, 0, sizeof(active_addr));
    if (get_active_address(NULL, active_addr, sizeof(active_addr)) != 0)
        return 1;

    /////////////////////////////////////////////////////
    memset(url, 0, 1024);
    strncat(url, "rtsp://", 7);
    strncat(url, active_addr, strlen(active_addr));
    snprintf(url + strlen(url), 1024, ":%d", PORT);
    alloc_rtsp_uri(url);
    /////////////////////////////////////////////////////

	base = event_base_new();
	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(active_addr);
	sin.sin_port = htons(PORT);

	listener = evconnlistener_new_bind(base, cc_listener_cb, (void *)base,
            LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)&sin, sizeof(sin));
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

	return 0;
}
