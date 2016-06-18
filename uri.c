#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uri.h"

struct list_head uri_list;
static pthread_mutex_t uri_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ref_uri(Uri *uri)
{
    if (!uri) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }

    pthread_mutex_lock(&uri->ref_mutex);
    switch (uri->status) {
    case URI_IDLE:
        ; // TODO: if (starting stream succed)
        uri->ref_counter++;
        uri->status = URI_BUSY;
        break;
    case URI_BUSY:
        uri->ref_counter++;
        break;
    case URI_IN_FREE:
        // never here
        break;
    }
    pthread_mutex_unlock(&uri->ref_mutex);
}

static void unref_uri(Uri *uri)
{
    if (!uri) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }

    pthread_mutex_lock(&uri->ref_mutex);
    switch (uri->status) {
    case URI_IDLE:
        break;
    case URI_BUSY:
        if (!--uri->ref_counter) {
            ; // TODO: if (stopping stream succed)
            uri->status = URI_IDLE;
        }
        break;
    case URI_IN_FREE:
        // never here
        break;
    }
    pthread_mutex_unlock(&uri->ref_mutex);
}

void init_uri_list()
{
    INIT_LIST_HEAD(&uri_list);
}

Uri* get_uri(char *url)
{
    Uri *uri = NULL, *p = NULL;
    if (!url)
        return NULL;

    pthread_mutex_lock(&uri_mutex);
    list_for_each_entry (p, &uri_list, list) {
        if (!strcmp(p->url, url)) {
            uri = p;
            break;
        }
    }
    if (uri) {
        if (uri->status != URI_IN_FREE)
            ref_uri(uri);
        else
            uri = NULL;
    }
    pthread_mutex_unlock(&uri_mutex);
    return uri;
}

void release_uri(Uri *uri)
{
    if (uri && uri->status != URI_IN_FREE)
        unref_uri(uri);
}

void alloc_uri(char *url, frame_opreation frame_opt)
{
    Uri *uri = NULL;
    if (!url) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }

    uri = (Uri*)calloc(1, sizeof(Uri));
    if (!uri) {
        printf("%s: calloc failed for uri\n", __func__);
        return;
    }
    pthread_mutex_init(&uri->ref_mutex, NULL);
    uri->status = URI_IDLE;

    uri->url = (char*)calloc(strlen(url) + 1, 1);
    if (!uri->url) {
        printf("%s: calloc failed for url of uri\n", __func__);
        free(uri);
        return;
    }
    strncpy(uri->url, url, strlen(url));
    uri->ssrc = rand();
    uri->frame_opt = frame_opt;

    pthread_mutex_lock(&uri_mutex);
    list_add_tail(&uri->list, &uri_list);
    pthread_mutex_unlock(&uri_mutex);
}

// return 0: success, -1: failed
static int free_uri(Uri *uri, int force)
{
    if (!uri)
        return -1;

    pthread_mutex_lock(&uri->ref_mutex);
    if (uri->status == URI_IDLE)
        uri->status = URI_IN_FREE;
    pthread_mutex_unlock(&uri->ref_mutex);

    if (uri->status == URI_IN_FREE) {
        pthread_mutex_lock(&uri_mutex);
        list_del(&uri->list);
        pthread_mutex_unlock(&uri_mutex);
    } else if (force) {
        ; // TODO: send ending uri signal
        pthread_mutex_lock(&uri_mutex);
        list_del(&uri->list);
        pthread_mutex_unlock(&uri_mutex);
    } else
        return -1;

    free(uri->url);
    free(uri);
    return 0;
}

void del_uris()
{
    Uri *uri = NULL;
    list_for_each_entry(uri, &uri_list, list)
        free_uri(uri, 0);
}
