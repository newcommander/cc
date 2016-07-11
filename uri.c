#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "uri.h"

struct list_head uri_list;
static pthread_mutex_t uri_mutex = PTHREAD_MUTEX_INITIALIZER;

extern int clean_uri_users(Uri *uri);
extern uri_entry entrys[];

int add_uri(char *url, uint32_t ssrc, frame_operation frame_opt)
{
    Uri *uri = NULL;
    if (!url) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    uri = (Uri*)calloc(1, sizeof(Uri));
    if (!uri) {
        printf("%s: calloc failed for uri\n", __func__);
        return -1;
    }

    uri->url = (char*)calloc(strlen(url) + 1, 1);
    if (!uri->url) {
        printf("%s: calloc failed for url of uri\n", __func__);
        free(uri);
        return -1;
    }
    strncpy(uri->url, url, strlen(url));

    pthread_mutex_init(&uri->ref_mutex, NULL);
    uri->ref_counter = 0;
    uri->status = URI_IDLE;
    uri->ssrc = ssrc;
    uri->frame_opt = frame_opt;
    INIT_LIST_HEAD(&uri->user_list);

    pthread_mutex_lock(&uri_mutex);
    list_add_tail(&uri->list, &uri_list);
    pthread_mutex_unlock(&uri_mutex);

    return 0;
}

static int release_uri(Uri *uri) {
    if (!uri) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    if (uri->url)
        free(uri->url);
    free(uri);

    return 0;
}

int del_uri(Uri *uri, int force)
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

Uri* find_uri(char *url)
{
    Uri *uri = NULL, *p = NULL;
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

int ref_uri(Uri *uri, struct list_head *list)
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

int unref_uri(Uri *uri, struct list_head *uri_user_list)
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

void uris_init(char *base_url)
{
    uri_entry *p = NULL;
    char url[1024];

    INIT_LIST_HEAD(&uri_list);
    for (p = entrys; p->func; p++) {
        memset(url, 0, 1024);
        if (p->title) {
            if (strlen(base_url) + 1 + strlen(p->title) + 1 > 1024)
                continue;
            strncat(url, base_url, strlen(base_url));
            strncat(url, "/", 1);
            strncat(url, p->title, strlen(p->title));
        } else {
            if (strlen(base_url) > 1023)
                break;
            strncat(url, base_url, strlen(base_url));
        }
        add_uri(url, rand(), p->func);
    }
}

void uris_deinit()
{
    struct list_head *list_p = NULL;
    Uri *uri = NULL;
    int ret = 0;

    for (list_p = uri_list.next; list_p != &uri_list; ) {
        uri = list_entry(list_p, Uri, list);
        list_p = list_p->next;
        ret = del_uri(uri, 1);
        if (ret < 0)
            printf("%s: delete uri(%s) failed\n", __func__, uri->url); // just warnning
    }
}

