#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <libgen.h>

#include "session.h"
#include "encoder.h"
#include "uri.h"

struct list_head uri_list;
static pthread_mutex_t uri_mutex = PTHREAD_MUTEX_INITIALIZER;

extern int clean_uri_users(struct Uri *uri);
extern struct uri_entry entrys[];
extern char active_addr[128];
extern char base_url[1024];

char *data_to_hex(char *buff, const uint8_t *src, int s, int lowercase)
{
    int i;
    static const char hex_table_uc[16] = { '0', '1', '2', '3',
                                           '4', '5', '6', '7',
                                           '8', '9', 'A', 'B',
                                           'C', 'D', 'E', 'F' };
    static const char hex_table_lc[16] = { '0', '1', '2', '3',
                                           '4', '5', '6', '7',
                                           '8', '9', 'a', 'b',
                                           'c', 'd', 'e', 'f' };
    const char *hex_table = lowercase ? hex_table_lc : hex_table_uc;

    for (i = 0; i < s; i++) {
        buff[i * 2]     = hex_table[src[i] >> 4];
        buff[i * 2 + 1] = hex_table[src[i] & 0xF];
    }
    return buff;
}

int make_sdp_string(struct Uri *uri)
{
    struct session se;
    struct timeval tv;

    char tmp[1024];
    char *version = "v=0\r\n";
    char origin[64];
    char *session_name = "s=live from cc\r\n";
    char *session_info = "i=hello, session infomation\r\n";
    char connection[64];
    char *time = "t=0 0\r\n";
    char session_attr[128];
    char *media_desc = "m=video 0 RTP/AVP 96\r\n";
    char *bandwidth_info = "b=AS:500\r\n";
    char *media_attr1 = "a=rtpmap:96 MP4V-ES/90000\r\n";
//    char *media_attr = "a=rtpmap:96 H264/90000\r\na=fmtp:96 packetization-mode=1;profile-level-id=64001F;sprop-parameter-sets=Z2QAH6zZQEAEmhAAAAMAEAAAAwMo8YMZYA==,aOvjyyLA\r\n";
    char media_attr2[512];
    char media_attr3[32];
    char media_attr4[32];
    char media_attr5[128];

    memset(&se, 0, sizeof(se));
    memset(origin, 0, sizeof(origin));
    memset(connection, 0, sizeof(connection));
    memset(session_attr, 0, sizeof(session_attr));
    memset(media_attr2, 0, sizeof(media_attr2));
    memset(media_attr3, 0, sizeof(media_attr3));
    memset(media_attr4, 0, sizeof(media_attr4));
    memset(media_attr5, 0, sizeof(media_attr5));

    gettimeofday(&tv, NULL);
    snprintf(origin, sizeof(origin), "o=- %ld.%ld 1 IN IP4 %s\r\n", tv.tv_sec, tv.tv_usec, active_addr);
    snprintf(connection, sizeof(connection), "c=IN IP4 %s\r\n", active_addr);
    strncat(session_attr, "a=range:npt=now-\r\n", 18);
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, uri->url, strlen(uri->url) > 1023 ? 1023 : strlen(uri->url));
    snprintf(session_attr + strlen(session_attr), sizeof(session_attr) - strlen(session_attr), "a=control:%s\r\n", dirname(tmp));

    //TODO: what about h264 codec ??
    snprintf(se.encoder_name, 6, "mpeg4");
    se.uri = uri;
    // open and then close codec, we got sdp string
    if (encoder_init(&se) < 0) {
        printf("%s: Cannot get sdp string\n", __func__);
        return -1;
    }
    if (sizeof(media_attr2) < se.cc->extradata_size * 2 + 1 + 17) {
        printf("%s: media attribute buffer for config_info is too small\n", __func__);
        return -1;
    }
    strncat(media_attr2, "a=fmtp:96 config=", 17);
    data_to_hex(media_attr2 + strlen(media_attr2), se.cc->extradata, se.cc->extradata_size, 0);
    strncat(media_attr2, "\r\n", 2);
    encoder_deinit(&se);

    snprintf(media_attr3, sizeof(media_attr3), "a=framerate:%d\r\n", uri->framerate);
    snprintf(media_attr4, sizeof(media_attr4), "a=framesize:96 %d-%d\r\n", uri->width, uri->height);
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, uri->url, strlen(uri->url) > 1023 ? 1023 : strlen(uri->url));
    snprintf(media_attr5, sizeof(media_attr5), "a=control:%s/%s\r\n", basename(tmp), uri->track);

    uri->sdp_str = (char*)calloc(strlen(version) + strlen(origin) + strlen(session_name) +
            strlen(session_info) + strlen(connection) + strlen(time) + strlen(session_attr) +
            strlen(media_desc) + strlen(bandwidth_info) + strlen(media_attr1) +
            strlen(media_attr2) + strlen(media_attr3) + strlen(media_attr4) + strlen(media_attr5), 1);
    strncat(uri->sdp_str, version, strlen(version));
    strncat(uri->sdp_str, origin, strlen(origin));
    strncat(uri->sdp_str, session_name, strlen(session_name));
    strncat(uri->sdp_str, session_info, strlen(session_info));
    strncat(uri->sdp_str, connection, strlen(connection));
    strncat(uri->sdp_str, time, strlen(time));
    strncat(uri->sdp_str, session_attr, strlen(session_attr));
    strncat(uri->sdp_str, media_desc, strlen(media_desc));
    strncat(uri->sdp_str, bandwidth_info, strlen(bandwidth_info));
    strncat(uri->sdp_str, media_attr1, strlen(media_attr1));
    strncat(uri->sdp_str, media_attr2, strlen(media_attr2));
    strncat(uri->sdp_str, media_attr3, strlen(media_attr3));
    strncat(uri->sdp_str, media_attr4, strlen(media_attr4));
    strncat(uri->sdp_str, media_attr5, strlen(media_attr5));

    return 0;
}

int add_uri(struct uri_entry *ue)
{
    struct Uri *uri = NULL;
    char url[1024];

    if (!ue || !ue->title || !ue->track || ue->screen_w <= 0 || ue->screen_h <= 0 || !ue->sample_func) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    memset(url, 0, sizeof(url));
    if (strlen(base_url) + 1 + strlen(ue->title) + 1 > sizeof(url)) {
        printf("%s: Uri's title too long: '%s/%s'\n", __func__, base_url, ue->title);
        return -1;
    }
    strncat(url, base_url, strlen(base_url));
    strncat(url, "/", 1);
    strncat(url, ue->title, strlen(ue->title));

    uri = (struct Uri*)calloc(1, sizeof(*uri));
    if (!uri) {
        printf("%s: calloc uri failed\n", __func__);
        return -1;
    }

    uri->url = (char*)calloc(strlen(url) + 1, 1);
    if (!uri->url) {
        printf("%s: calloc url of uri failed\n", __func__);
        free(uri);
        return -1;
    }
    snprintf(uri->url, strlen(url) + 1, url);

    uri->status = URI_IDLE;
    INIT_LIST_HEAD(&uri->user_list);
    pthread_mutex_init(&uri->ref_mutex, NULL);
    uri->ref_counter = 0;
    uri->ssrc = (uint32_t)rand();
    uri->width = ue->screen_w;
    uri->height = ue->screen_h;
    uri->framerate = ue->framerate;
    uri->track = ue->track;
    uri->sample_func = ue->sample_func;
    make_sdp_string(uri);
    if (!uri->sdp_str) {
        printf("%s: Add uri[url:%s] failed\n", __func__, uri->url);
        return -1;
    }

    pthread_mutex_lock(&uri_mutex);
    list_add_tail(&uri->list, &uri_list);
    pthread_mutex_unlock(&uri_mutex);

    return 0;
}

static int release_uri(struct Uri *uri) {
    if (!uri) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    if (uri->url)
        free(uri->url);
    if (uri->sdp_str)
        free(uri->sdp_str);
    free(uri);

    return 0;
}

int del_uri(struct Uri *uri, int force)
{
    int ret = 0;

    if (!uri) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    pthread_mutex_lock(&uri->ref_mutex);
    if (uri->status == URI_IDLE)
        uri->status = URI_IN_FREE;
    pthread_mutex_unlock(&uri->ref_mutex);

    // here, insure the uri status == URI_IN_FREE or URI_BUSY
    if (uri->status == URI_IN_FREE) {
        pthread_mutex_lock(&uri_mutex);
        list_del(&uri->list);
        pthread_mutex_unlock(&uri_mutex);
    } else if (force) {
        ret = clean_uri_users(uri);
        if (ret < 0) {
            printf("%s: Cleaning uri(%s)'s users in force failed, uri could not be deleted\n", __func__, uri->url);
            return -1;
        }
        pthread_mutex_lock(&uri_mutex);
        list_del(&uri->list);
        pthread_mutex_unlock(&uri_mutex);
    } else {
        // uri->status == URI_BUSY
        printf("%s: uri(%s) is busy, could not be deleted\n", __func__, uri->url);
        return -1;
    }

    release_uri(uri);
    return 0;
}

struct Uri* find_uri(char *url)
{
    struct Uri *uri = NULL, *p = NULL;
    if (!url) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }

    pthread_mutex_lock(&uri_mutex);
    list_for_each_entry(p, &uri_list, list) {
        if (!strcmp(p->url, url)) {
            uri = p;
            break;
        }
    }
    if (uri && uri->status == URI_IN_FREE)
        uri = NULL;
    pthread_mutex_unlock(&uri_mutex);
    return uri;
}

int ref_uri(struct Uri *uri, struct list_head *list)
{
    int ret = 0;
    if (!uri || !list) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    pthread_mutex_lock(&uri->ref_mutex);
    switch (uri->status) {
    case URI_IDLE:
        uri->status = URI_BUSY;
    case URI_BUSY:
        uri->ref_counter++;
        list_add_tail(list, &uri->user_list);
        break;
    case URI_IN_FREE:
        ret = -1;
        break;
    }
    pthread_mutex_unlock(&uri->ref_mutex);

    return ret;
}

int unref_uri(struct Uri *uri, struct list_head *uri_user_list)
{
    int ret = 0;
    if (!uri || !uri_user_list) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    pthread_mutex_lock(&uri->ref_mutex);
    switch (uri->status) {
    case URI_IDLE:
        break;
    case URI_BUSY:
        list_del(uri_user_list);
        if (!--uri->ref_counter)
            uri->status = URI_IDLE;
        break;
    case URI_IN_FREE:
        // never here
        ret = -1;
        break;
    }
    pthread_mutex_unlock(&uri->ref_mutex);

    return ret;
}

void uris_init()
{
    struct uri_entry *ue = NULL;

    INIT_LIST_HEAD(&uri_list);

    for (ue = entrys; ue->title; ue++) {
        add_uri(ue);
    }
}

void uris_deinit()
{
    struct list_head *list_p = NULL;
    struct Uri *uri = NULL;
    int ret = 0;

    for (list_p = uri_list.next; list_p != &uri_list; ) {
        uri = list_entry(list_p, struct Uri, list);
        list_p = list_p->next;
        ret = del_uri(uri, 1);
        if (ret < 0)
            printf("%s: delete uri(%s) failed\n", __func__, uri->url); // just warnning
    }
}

