// RTSP(rfc2326) https://tools.ietf.org/html/rfc2326

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rtsp.h"

static status_code response_code[] = {
    { "100", "Continue" },
    { "200", "OK" },
    { "201", "Created" },
    { "250", "Low on Storage Space" },
    { "300", "Multiple Choices" },
    { "301", "Moved Permanently" },
    { "302", "Moved Temporarily" },
    { "303", "See Other" },
    { "304", "Not Modified" },
    { "305", "Use Proxy" },
    { "400", "Bad Request" },
    { "401", "Unauthorized" },
    { "402", "Payment Required" },
    { "403", "Forbidden" },
    { "404", "Not Found" },
    { "405", "Method Not Allowed" },
    { "406", "Not Acceptable" },
    { "407", "Proxy Authentication Required" },
    { "408", "Request Time-out" },
    { "410", "Gone" },
    { "411", "Length Required" },
    { "412", "Precondition Failed" },
    { "413", "Request Entity Too Large" },
    { "414", "Request-URI Too Large" },
    { "415", "Unsupported Media Type" },
    { "451", "Parameter Not Understood" },
    { "452", "Conference Not Found" },
    { "453", "Not Enough Bandwidth" },
    { "454", "Session Not Found" },
    { "455", "Method Not Valid in This State" },
    { "456", "Header Field Not Valid for Resource" },
    { "457", "Invalid Range" },
    { "458", "Parameter Is Read-Only" },
    { "459", "Aggregate operation not allowed" },
    { "460", "Only aggregate operation allowed" },
    { "461", "Unsupported transport" },
    { "462", "Destination unreachable" },
    { "500", "Internal Server Error" },
    { "501", "Not Implemented" },
    { "502", "Bad Gateway" },
    { "503", "Service Unavailable" },
    { "504", "Gateway Time-out" },
    { "505", "RTSP Version not supported" },
    { "551", "Option not supported" },
    { NULL, NULL }
};

static char active_addr[128];
static const int PORT = 554;
static char base_url[1024];

static struct list_head session_list;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    printf("%s: No element found\n", __func__);
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
    } else if (code && phrase) {
        *buf = (char*)calloc(13 + strlen(code) + strlen(phrase), 1);
        if (!*buf)
            goto calloc_failed;
    } else if (code && !phrase) {
        phrase = find_phrase_by_code(code);
        if (!phrase)
            return -1;
        *buf = (char*)calloc(13 + strlen(code) + strlen(phrase), 1);
        if (!*buf)
            goto calloc_failed;
    } else {
        code = find_code_by_phrase(phrase);
        if (!code)
            return -1;
        *buf = (char*)calloc(13 + strlen(code) + strlen(phrase), 1);
        if (!(*buf))
            goto calloc_failed;
    }
    sprintf(*buf, "RTSP/1.0 %s %s\r\n", code, phrase);
    return 0;

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

    make_status_line(&status_line, "200", NULL);
    if (!status_line) {
        ret = 500;
        goto out;
    }
    make_response_cseq(&cseq_str, cseq);
    if (!cseq_str) {
        ret = 500;
        goto out;
    }
    make_response_date(&date_str);
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

/* NOTE: please call bufferevent_free(rs->bev) according your situation */
static void release_rtsp_session(rtsp_session *rs)
{
    void *res = NULL;
    int ret = 0;

    if (!rs)
        return;

    switch(rs->status) {
    case SESION_READY:
        unref_uri(rs->uri, &rs->uri_user_list);
        rs->status = SESION_IN_FREE;
        break;
    case SESION_PLAYING:
        unref_uri(rs->uri, &rs->uri_user_list);
        if (rs->rtcp_thread) {
            uv_udp_recv_stop(&rs->rtcp_handle);
            uv_stop(&rs->rtcp_loop);
            ret = pthread_join(rs->rtcp_thread, &res);
            if (ret != 0)
                printf("%s: pthread_join for rtcp_thread return error: %s\n", __func__, strerror(ret)); // TODO: and what ?
            rs->rtcp_thread = 0;
        }
        rs->status = SESION_IN_FREE;
        break;
    case SESION_IN_FREE:
        break;
    }

    if (rs->status != SESION_IN_FREE) {
        printf("%s: Could not release RTSP session on %s\n", __func__, rs->uri->url);
        return;
    }

    pthread_mutex_lock(&session_mutex);
    list_del(&rs->list);
    pthread_mutex_unlock(&session_mutex);

    free(rs);
}

static rtsp_session* create_rtsp_session(rtsp_request *rr, int c_rtp_port, int c_rtcp_port)
{
    struct timeval tv;
    struct sockaddr clit_addr;
    struct sockaddr_in *clit_addr_in;
    socklen_t addr_len = sizeof(struct sockaddr);
    rtsp_session *rs = NULL;
    Uri *uri = NULL;
    int port, ret;
    char clit_ip[16];

    if (!rr || !rr->bev) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }

    uri = find_uri(rr->url);
    if (!uri) {
        printf("%s: No uri(%s) found\n", __func__, rr->url);
        return NULL;
    }

    ret = getpeername(rr->bev->ev_read.ev_fd, &clit_addr, &addr_len);
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

    rs = (rtsp_session*)calloc(1, sizeof(rtsp_session));
    if (!rs) {
        printf("%s: calloc failed\n", __func__);
        return NULL;
    }
    rs->uri = uri;
    rs->bev = rr->bev;
    rs->bev->wm_read.private_data = rs;
    uv_ip4_addr(clit_ip, c_rtp_port, &rs->clit_rtp_addr);
    uv_ip4_addr(clit_ip, c_rtcp_port, &rs->clit_rtcp_addr);

    ret = uv_loop_init(&rs->rtp_loop);
    if (ret != 0) {
        printf("%s: init RTP UDP loop failed: %s\n", __func__, uv_strerror(ret));
        goto failed_rtp_loop_init;
    }
    ret = uv_loop_init(&rs->rtcp_loop);
    if (ret != 0) {
        printf("%s: init RTCP UDP loop failed: %s\n", __func__, uv_strerror(ret));
        goto failed_rtcp_loop_init;
    }

    ret = uv_udp_init(&rs->rtp_loop, &rs->rtp_handle);
    if (ret != 0) {
        printf("%s: init RTP UDP handle failed: %s\n", __func__, uv_strerror(ret));
        goto failed;
    }
    ret = uv_udp_init(&rs->rtcp_loop, &rs->rtcp_handle);
    if (ret != 0) {
        printf("%s: init RTCP UDP handle failed: %s\n", __func__, uv_strerror(ret));
        goto failed;
    }

    port = 1025;
    ret = 1;
    while (ret) {
        uv_ip4_addr(active_addr, port++, &rs->serv_rtp_addr);
        ret = uv_udp_bind(&rs->rtp_handle, (struct sockaddr*)&rs->serv_rtp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret)
            goto failed;
        uv_ip4_addr(active_addr, port++, &rs->serv_rtcp_addr);
        ret = uv_udp_bind(&rs->rtcp_handle, (struct sockaddr*)&rs->serv_rtcp_addr, 0);
        if (ret == UV_EADDRINUSE)
            continue;
        else if (ret)
            goto failed;

        if (port > 65534)
            goto failed;

        break;
    }
    gettimeofday(&tv, NULL);
    snprintf(rs->session_id, 9, "%04x%04x", (unsigned int)tv.tv_usec, (unsigned int)rand());
    rs->status = SESION_READY;

    ret = ref_uri(rs->uri, &rs->uri_user_list);
    if (ret < 0) {
        printf("%s: Cannot reference uri(%s)\n", __func__, rs->uri->url);
        goto failed;
    }

    pthread_mutex_lock(&session_mutex);
    list_add(&rs->list, &session_list);
    pthread_mutex_unlock(&session_mutex);

    return rs;

// failed_ref: never here
    unref_uri(uri, &rs->uri_user_list);
failed:
    uv_loop_close(&rs->rtcp_loop);
failed_rtcp_loop_init:
    uv_loop_close(&rs->rtp_loop);
failed_rtp_loop_init:
    release_rtsp_session(rs);
    return NULL;
}

int clean_uri_users(Uri *uri)
{
    struct list_head *list_p = NULL;
    rtsp_session *rs = NULL;

    if (!uri) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    for (list_p = uri->user_list.next; list_p != &uri->user_list; ) {
        rs = list_entry(list_p, rtsp_session, uri_user_list);
        list_p = list_p->next;
        bufferevent_free(rs->bev);
        release_rtsp_session(rs);
    }

    return 0;
}

static rtsp_session* find_rtsp_session_by_id(char *session_id)
{
    rtsp_session *rs = NULL, *p = NULL;
    if (!session_id)
        return NULL;
    pthread_mutex_lock(&session_mutex);
    list_for_each_entry(p, &session_list, list) {
        if (!strcmp(p->session_id, session_id)) {
            rs = p;
            if (rs->status == SESION_IN_FREE)
                rs = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&session_mutex);
    return rs;
}

static int make_sdp_string(char **buf)
{
    char *version = "v=0\r\n";
    char *origin = "o=- 1 1 IN IP4 0.0.0.0\r\n";
    char *session_name = "s=live from cc\r\n";
    char *session_info = "i=hello, session infomation\r\n";
    char *connection_info = "c=IN IP4 192.168.0.123\r\n";
    char *time = "t=0 0\r\n";
    char *session_attr = "a=range:npt=now-\r\na=control:rtsp://192.168.0.123/\r\n";
    //char *session_control = "a=control:" + base_url +  "\r\n";
    char *media_desc = "m=video 0 RTP/AVP 96\r\n";
    char *bandwidth_info = "b=AS:500\r\n";
    char *media_attr1 = "a=rtpmap:96 MP4V-ES/90000\r\n";
    //char *media_attr = "a=rtpmap:96 H264/90000\r\na=fmtp:96 packetization-mode=1;profile-level-id=64001F;sprop-parameter-sets=Z2QAH6zZQEAEmhAAAAMAEAAAAwMo8YMZYA==,aOvjyyLA\r\n";
    char *media_attr2 = "a=fmtp:96 config=000001B001000001B58913000001000000012000C48D8800CD12C4321463000001B24C61766335372E32342E313032\r\n";
    char *media_attr3 = "a=framerate:25\r\n";
    char *media_attr4 = "a=framesize:96 600-400\r\n";
    char *media_attr5 = "a=control:trackID=1\r\n";
    struct timeval tv;

    if (!buf) {
        printf("%s: Invalid parameter\n", __func__);
        *buf = NULL;
        return -1;
    }

    gettimeofday(&tv, NULL);

    /*
    char origin[1024];
    memset(origin, 0, 1024);
    strncat(origin, "o=- ", 4);
    snprintf(origin + strlen(origin), 20, "%ld.%ld ", tv.tv_sec, tv.tv_usec);
    strncat(origin, "1 IN IP4 ", 9);
    strncat(origin, active_addr, strlen(active_addr));
    strncat(origin, "\r\n", 2);
    printf("[%ld]%s\n", strlen(origin), origin);
    */

    *buf = (char*)calloc(strlen(version) + strlen(origin) + strlen(session_name) + strlen(session_info) + \
            strlen(connection_info) + strlen(time) + strlen(session_attr) + strlen(media_desc) + \
            strlen(bandwidth_info) + strlen(media_attr1) + strlen(media_attr2) + strlen(media_attr3) + \
            strlen(media_attr4) + strlen(media_attr5), 1);
    if (!(*buf)) {
        printf("%s: calloc failed\n", __func__);
        return -1;
    }
    strncat(*buf, version, strlen(version));
    strncat(*buf, origin, strlen(origin));
    strncat(*buf, session_name, strlen(session_name));
    strncat(*buf, session_info, strlen(session_info));
    strncat(*buf, connection_info, strlen(connection_info));
    strncat(*buf, time, strlen(time));
    strncat(*buf, session_attr, strlen(session_attr));
    strncat(*buf, media_desc, strlen(media_desc));
    strncat(*buf, bandwidth_info, strlen(bandwidth_info));
    strncat(*buf, media_attr1, strlen(media_attr1));
    strncat(*buf, media_attr2, strlen(media_attr2));
    strncat(*buf, media_attr3, strlen(media_attr3));
    strncat(*buf, media_attr4, strlen(media_attr4));
    strncat(*buf, media_attr5, strlen(media_attr5));

    return 0;
}

static int make_entity_header(Uri *uri, char **buf, int sdp_len)
{
    char sdp_len_str[32];
    if (!uri || !buf || (sdp_len < 0)) {
        printf("%s: Invalid parameter\n", __func__);
        *buf = NULL;
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

static int make_response_for_describe(rtsp_request *rr, char **response)
{
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL, *entity_header = NULL, *sdp_str = NULL;
    int len = 0;
    Uri *uri = NULL;

    if (!response || !rr || !rr->rh.accept) {
        printf("%s: Invalid parameter\n", __func__);
        *response = NULL;
        return 500;
    }

    if (!strstr(rr->rh.accept, "application/sdp")) {
        printf("%s: Only accept application/sdp, but client require %s\n", __func__, rr->rh.accept);
        *response = NULL;
        return 406;
    }

    uri = find_uri(rr->url);
    if (!uri) {
        printf("%s: No uri(%s) found\n", __func__, rr->url);
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
        p_tmp = strstr(p_head, "=");
        if (!p_tail)
            p_tail = rr->rh.transport + len;
        if (!strncmp(p_head, "RTP/AVP/UDP", p_tail - p_head))
            protocol = "RTP/AVP/UDP";
        else if (!strncmp(p_head, "unicast", p_tail - p_head))
            mode = "unicast";
        else if (p_tmp) {
            if (!strncmp(p_head, "client_port", p_tmp - p_head)) {
                p_head = ++p_tmp;
                p_tmp = strstr(p_head, "-");
                if (p_tmp) {
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

    rs = create_rtsp_session(rr, c_rtp_port, c_rtcp_port);
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
            c_rtp_port, c_rtcp_port, ntohs(rs->serv_rtp_addr.sin_port), ntohs(rs->serv_rtcp_addr.sin_port));
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

extern void* rtcp_dispatch(void *arg);
static int make_response_for_play(rtsp_request *rr, char **response)
{
    rtsp_session *rs = NULL;
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL;
    int len;

    rs = find_rtsp_session_by_id(rr->rh.session_id);
    if (!rs)
        return 454;

    if (rs->status == SESION_PLAYING) {
        printf("%s: Session already in PLAYING status\n", __func__);
        return 455;
    }
    rs->status = SESION_PLAYING;

    // play
    if (pthread_create(&rs->rtcp_thread, NULL, rtcp_dispatch, rs) != 0) {
        printf("%s: creating rtcp thread failed: %s\n", __func__, strerror(errno));
        rs->rtcp_thread = 0;
        return 500;
    }

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
        printf("%s: Unknow RTSP request method (METHOD: %d)\n", __func__, rr->method);
        return 405;
    }
    return ret;
}

static int get_request_line(rtsp_request *rr, char *buf, int len)
{
    char *p_head = NULL, *p_tail = NULL, *tmp = NULL, request_url[1024];

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
        return 405;
    } else if (!strncmp("TEARDOWN", p_head, p_tail - p_head)) {
        rr->method = MTH_TEARDOWN;
    } else if (!strncmp("RECORD", p_head, p_tail - p_head)) {
        return 405;
    } else if (!strncmp("ANNOUNCE", p_head, p_tail - p_head)) {
        return 405;
    } else if (!strncmp("GET_PARAMETER", p_head, p_tail - p_head)) {
        return 405;
    } else if (!strncmp("REDIRECT", p_head, p_tail - p_head)) {
        return 405;
    } else if (!strncmp("SET_PARAMETER", p_head, p_tail - p_head)) {
        return 405;
    } else {
        printf("%s: Unknow RTSP request recived: %s\n", __func__, p_head);
        return 400;
    }

    for (p_head = ++p_tail; *p_tail != ' ' && p_tail != buf + len - 1; p_tail++) ;
    if (*p_tail != ' ') {
        printf("Invalid RTSP request when get url\n");
        return 400;
    }

    if (strncmp("rtsp://", p_head, 7)) {
        printf("Invalid protocol: %s\n", p_head);
        return 400;
    }
    for (tmp = p_head + 7; tmp < p_tail; tmp++) {
        if (*tmp == ':')
            break;
    }
    memset(request_url, 0, 1024);
    memcpy(request_url, p_head, tmp - p_head);
    if (*tmp == ':') {
        for (; tmp <= p_tail; tmp++) {
            if (*tmp == '/') {
                strncat(request_url, tmp, p_tail - tmp);
                break;
            }
        }
    }
    rr->url = (char*)calloc(strlen(request_url) + 1, 1);
    if (!rr->url) {
        printf("%s: calloc for url failed\n", __func__);
        return 500;
    }
    strncpy(rr->url, request_url, strlen(request_url));

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

static void error_reply(int code, int cseq, char **response)
{
    char *status_line = NULL, *cseq_str = NULL, *date_str = NULL;
    char code_buf[32];

    *response = NULL;
    if (code <= 0) {
        printf("%s: Bad status code: %d\n", __func__, code);
        ; // TODO
    }
    snprintf(code_buf, 32, "%d", code);

    make_status_line(&status_line, code_buf, NULL);
    if (!status_line) {
        printf("%s: error", __func__);
        goto out;
    }
    make_response_cseq(&cseq_str, cseq);
    if (!cseq_str) {
        printf("%s: error", __func__);
        goto out;
    }
    make_response_date(&date_str);
    if (!cseq_str) {
        printf("%s: error", __func__);
        goto out;
    }
    *response = (char*)calloc(strlen(status_line) + strlen(cseq_str) + strlen(date_str) + 13, 1);
    if (!(*response)) {
        printf("%s: calloc failed\n", __func__);
        goto out;
    }
    snprintf(*response, strlen(status_line) + 1, status_line);
    strncat(*response, cseq_str, strlen(cseq_str));
    strncat(*response, date_str, strlen(date_str));
    strncat(*response, "\r\n", 2);

out:
    if (status_line)
        free(status_line);
    if (cseq_str)
        free(cseq_str);
    if (date_str)
        free(date_str);
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
    if (ret != 0)
        return ret;

    ret = get_rtsp_request_header(*rr, buf, len);
    if (ret != 0)
        return ret;

    return 0;
}

static void release_rtsp_request(rtsp_request *rr)
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
    free(rr);
}

static void read_cb(struct bufferevent *bev, void *user_data)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    rtsp_request *rr = NULL;
    rtsp_session *rs = NULL;
    char *response_str = NULL, *p = NULL;
    int ret = 0, offset = 0;

    size_t len = evbuffer_get_length(input);
    char *buf = (char*)calloc(len + 1, 1);
    if (!buf) {
        printf("%s: calloc failed\n", __func__);
        error_reply(500, 0, &response_str);
        goto reply;
    }

    p = buf;
    offset = 0;
    while ((offset = bufferevent_read(bev, p, buf + len - p)) > 0)
        p += offset;

    ret = convert_rtsp_request(&rr, bev, buf, len);
    if (ret != 0 || !rr) {
        error_reply(ret, rr ? rr->rh.cseq : 0, &response_str);
        goto reply;
    }

    ret = make_response(rr, &response_str);
    if (ret != 0) {
        if (response_str) {
            free(response_str);
            response_str = NULL;
        }
        error_reply(ret, rr->rh.cseq, &response_str);
    }

reply:
    if (response_str) {
        bufferevent_write(bev, response_str, strlen(response_str));
        bufferevent_flush(bev, EV_WRITE, BEV_FLUSH);
        free(response_str);
    }

    if (ret != 0 || rr->method == MTH_TEARDOWN) {
        rs = (rtsp_session*)bev->wm_read.private_data;
        if (rs)
            release_rtsp_session(rs);
        bufferevent_free(bev);
    }

    release_rtsp_request(rr);
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
    rtsp_session *rs = NULL;

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
            break;
        }
        return;
    }

    rs = (rtsp_session*)bev->wm_read.private_data;
    if (rs)
        release_rtsp_session(rs);
    bufferevent_free(bev);
}

static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *user_data)
{
    struct event_base *base = user_data;
    struct bufferevent *bev;

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
    struct event_base *base = user_data;
    struct timeval delay = { 1, 0 };
    struct list_head *list_p = NULL;
    rtsp_session *rs = NULL;

    for (list_p = session_list.next; list_p != &session_list; ) {
        rs = list_entry(list_p, rtsp_session, list);
        list_p = list_p->next;
        bufferevent_free(rs->bev);
        release_rtsp_session(rs);
    }
    printf("%s: Caught an interrupt signal; exiting cleanly in 1 second.\n", __func__);
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

    if (getifaddrs(&iflist)) {
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

void* cc_stream(void *arg)
{
    struct event_base *base;
    struct evconnlistener *listener;
    struct event *signal_event;
    struct sockaddr_in sin;
    char *nic_name = (char*)arg;

    INIT_LIST_HEAD(&session_list);

    memset(active_addr, 0, sizeof(active_addr));
    if (get_active_address(nic_name, active_addr, sizeof(active_addr)) != 0)
        return NULL;

    memset(base_url, 0, sizeof(base_url));
    strncat(base_url, "rtsp://", 7);
    strncat(base_url, active_addr, strlen(active_addr));
    uris_init(base_url);

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return NULL;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(active_addr);
    sin.sin_port = htons(PORT);

    listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
            LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)&sin, sizeof(sin));
    if (!listener) {
        fprintf(stderr, "Could not create a listener!\n");
        return NULL;
    }

    signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
    if (!signal_event || event_add(signal_event, NULL) < 0) {
        fprintf(stderr, "Could not create/add a signal event!\n");
        return NULL;
    }

    event_base_dispatch(base);

    uris_deinit();

    evconnlistener_free(listener);
    event_free(signal_event);
    event_base_free(base);

    return NULL;
}
