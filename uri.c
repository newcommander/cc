#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <libgen.h>

#include "encoder.h"
#include "uri.h"

struct list_head uri_list;
static pthread_mutex_t uri_list_mutex = PTHREAD_MUTEX_INITIALIZER;

extern char active_addr[];
extern char base_url[];

#define CONFIG_BUF_SIZE 2048
int make_sdp_string(struct Uri *uri, char *encoder_name)
{
    struct timeval tv;
    char *sdp_str = NULL;

    char *tmp_str = NULL;
    char *version = "v=0\r\n";
    char origin[64];
    char *session_name = "s=live from cc\r\n";
    char *session_info = "i=hello, session infomation\r\n";
    char connection[64];
    char *time = "t=0 0\r\n";
    char session_attr[128];
    char *media_desc = "m=video 0 RTP/AVP 96\r\n";
    char *bandwidth_info = "b=AS:500\r\n";
    char *media_attr1 = NULL;
    char media_attr2[CONFIG_BUF_SIZE];
    char media_attr3[32];
    char media_attr4[32];
    char media_attr5[128];

    if (!uri || !encoder_name) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

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
    tmp_str = strdup(uri->url);
    snprintf(session_attr + strlen(session_attr), sizeof(session_attr) - strlen(session_attr), "a=control:%s\r\n", dirname(tmp_str));
    free(tmp_str);

    if (!strncmp(encoder_name, "mpeg4", 5)) {
        media_attr1 = "a=rtpmap:96 MP4V-ES/90000\r\n";
        strncat(media_attr2, "a=fmtp:96 ", 10);
        get_media_config(uri, encoder_name, media_attr2 + 10, CONFIG_BUF_SIZE - 13);
    } else if (!strncmp(encoder_name, "h264", 4)) {
        media_attr1 = "a=rtpmap:96 H264/90000\r\n";
        strncat(media_attr2, "a=fmtp:96 packetization-mode=1", 30);
        get_media_config(uri, encoder_name, media_attr2 + 30, CONFIG_BUF_SIZE - 33);
    } else {
        printf("%s: Unknow encoder name: %s\n", __func__, encoder_name);
        return -1;
    }
    strncat(media_attr2, "\r\n", 2);

    snprintf(media_attr3, sizeof(media_attr3), "a=framerate:%d\r\n", uri->framerate);
    snprintf(media_attr4, sizeof(media_attr4), "a=framesize:96 %d-%d\r\n", uri->width, uri->height);
    tmp_str = strdup(uri->url);
    snprintf(media_attr5, sizeof(media_attr5), "a=control:%s/%s\r\n", basename(tmp_str), uri->track);
    free(tmp_str);

    sdp_str = (char*)calloc(strlen(version) + strlen(origin) + strlen(session_name) +
            strlen(session_info) + strlen(connection) + strlen(time) + strlen(session_attr) +
            strlen(media_desc) + strlen(bandwidth_info) + strlen(media_attr1) +
            strlen(media_attr2) + strlen(media_attr3) + strlen(media_attr4) + strlen(media_attr5) + 1, 1);
    if (!sdp_str) {
        printf("%s: calloc failed\n", __func__);
        return -1;
    }
    strncat(sdp_str, version, strlen(version));
    strncat(sdp_str, origin, strlen(origin));
    strncat(sdp_str, session_name, strlen(session_name));
    strncat(sdp_str, session_info, strlen(session_info));
    strncat(sdp_str, connection, strlen(connection));
    strncat(sdp_str, time, strlen(time));
    strncat(sdp_str, session_attr, strlen(session_attr));
    strncat(sdp_str, media_desc, strlen(media_desc));
    strncat(sdp_str, bandwidth_info, strlen(bandwidth_info));
    strncat(sdp_str, media_attr1, strlen(media_attr1));
    strncat(sdp_str, media_attr2, strlen(media_attr2));
    strncat(sdp_str, media_attr3, strlen(media_attr3));
    strncat(sdp_str, media_attr4, strlen(media_attr4));
    strncat(sdp_str, media_attr5, strlen(media_attr5));

    if (!strncmp(encoder_name, "mpeg4", 5)) {
        uri->sdp_str_mpeg4 = sdp_str;
    } else if (!strncmp(encoder_name, "h264", 4)) {
        uri->sdp_str_h264 = sdp_str;
    } else {
        printf("%s: Unknow error when making sdp string\n", __func__);
        free(sdp_str);
        return -1;
    }

    return 0;
}

int add_uri(struct uri_entry *ue)
{
    struct Uri *uri = NULL;
    char url[1024];

    if (!ue) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    if (!ue->title || !ue->track || !ue->sample_func || ue->screen_w <= 0 || ue->screen_h <= 0) {
        printf("%s: Invalid uri entry: track=%s,sample_func=%p,screen_width=%d,screen_height=%d\n",
                __func__, ue->track, ue->sample_func, ue->screen_w, ue->screen_h);
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

    uri = (struct Uri*)calloc(1, sizeof(struct Uri));
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

    INIT_LIST_HEAD(&uri->user_list);
    pthread_mutex_init(&uri->user_list_mutex, NULL);
    uri->user_num = 0;
    uri->ssrc = (uint32_t)rand();
    uri->width = ue->screen_w;
    uri->height = ue->screen_h;
    uri->framerate = ue->framerate;
    uri->track = ue->track;
    uri->sample_func = ue->sample_func;
    if (make_sdp_string(uri, "mpeg4") < 0) {
        printf("%s: failed making mpeg4 sdp string\n", __func__);
        free(uri->url);
        free(uri);
        return -1;
    }
    if (make_sdp_string(uri, "h264") < 0) {
        printf("%s: failed making h264 sdp string\n", __func__);
        free(uri->url);
        free(uri);
        return -1;
    }

    pthread_mutex_lock(&uri_list_mutex);
    list_add_tail(&uri->list, &uri_list);
    pthread_mutex_unlock(&uri_list_mutex);

    return 0;
}

void del_uri(struct Uri *uri)
{
    struct session *se = NULL;

    pthread_mutex_lock(&uri_list_mutex);
    list_del(&uri->list);
    pthread_mutex_unlock(&uri_list_mutex);

    pthread_mutex_lock(&uri->user_list_mutex);
    if (!list_empty(&uri->user_list)) {
        se = list_first_entry(&uri->user_list, struct session, uri_user_list);
        list_del(&se->uri_user_list);
        uri->user_num--;
        session_destroy(se);
    }
    pthread_mutex_unlock(&uri->user_list_mutex);

    if (uri->url)
        free(uri->url);
    if (uri->sdp_str_mpeg4)
        free(uri->sdp_str_mpeg4);
    if (uri->sdp_str_h264)
        free(uri->sdp_str_h264);
    free(uri);
}

struct Uri* find_uri(char *url)
{
    struct Uri *uri = NULL, *p = NULL;

    if (!url) {
        printf("%s: Invalid parameter\n", __func__);
        return NULL;
    }

    pthread_mutex_lock(&uri_list_mutex);
    list_for_each_entry(p, &uri_list, list) {
        if (!strcmp(p->url, url)) {
            uri = p;
            break;
        }
    }
//    if (uri && uri->status == URI_IN_FREE)
//        uri = NULL;
    pthread_mutex_unlock(&uri_list_mutex);

    return uri;
}

int ref_uri(struct session *se)
{
    if (!se || !se->uri) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    // TODO: what if this uri is freeing ??
    pthread_mutex_lock(&se->uri->user_list_mutex);
    list_add_tail(&se->uri_user_list, &se->uri->user_list);
    se->uri->user_num++;
    pthread_mutex_unlock(&se->uri->user_list_mutex);

    return 0;
}

int unref_uri(struct session *se)
{
    if (!se || !se->uri) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    // TODO: what if this uri is freeing ??
    pthread_mutex_lock(&se->uri->user_list_mutex);
    list_del(&se->uri_user_list);
    se->uri->user_num--;
    pthread_mutex_unlock(&se->uri->user_list_mutex);

    return 0;
}

void uris_init(struct uri_entry *entrys)
{
    struct uri_entry *ue = NULL;
    int ret = 0;

    INIT_LIST_HEAD(&uri_list);

    for (ue = entrys; ue->title; ue++) {
        ret = add_uri(ue);
        if (ret < 0)
            printf("%s: Add uri failed, title: %s, track: %s\n", __func__, ue->title, ue->track);
    }
}

void uris_deinit()
{
    struct Uri *uri = NULL;

    while (!list_empty(&uri_list)) {
        uri = list_first_entry(&uri_list, struct Uri, list);
        del_uri(uri);
    }
}

