#ifndef URI_H
#define URI_H

#include "common.h"
#include "list.h"

typedef struct {
    struct list_head list;
    pthread_mutex_t ref_mutex;
    struct list_head user_list;
    int ref_counter;
    char *url;
#define URI_IDLE 0
#define URI_BUSY 1
#define URI_IN_FREE 2
    int status;
    uint32_t ssrc;
    frame_operation frame_opt;
	int width;
	int height
} Uri;

int add_uri(char *url, uint32_t ssrc, frame_operation frame_opt);
int del_uri(Uri *uri, int force);
Uri* find_uri(char *url);
int ref_uri(Uri *uri, struct list_head *list);
int unref_uri(Uri *uri, struct list_head *list);
void uris_init(char *base_url);
void uris_deinit();

#endif
