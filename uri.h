#ifndef URI_H
#define URI_H

#include <pthread.h>
#include <stdint.h>

#include "list.h"

typedef int (*sample_function)(void *_frame, int screen_w, int screen_h);

struct Uri {
#define URI_IDLE 0
#define URI_BUSY 1
#define URI_IN_FREE 2
    int status;
    struct list_head list;
    struct list_head user_list;
    pthread_mutex_t ref_mutex;
    int ref_counter;
    uint32_t ssrc;
    char *sdp_str;
    char *url;
	int width;
	int height;
    int framerate;
    char *track;
    sample_function sample_func;
};

struct uri_entry {
    char *title;
    char *track;
    int screen_w;
    int screen_h;
    int framerate;
    sample_function sample_func;
};

extern int add_uri(struct uri_entry *ue);
//extern int add_uri(char *url, uint32_t ssrc, sample_function sample_func);
extern int del_uri(struct Uri *uri, int force);
extern struct Uri* find_uri(char *url);
extern int ref_uri(struct Uri *uri, struct list_head *list);
extern int unref_uri(struct Uri *uri, struct list_head *list);
extern void uris_init();
extern void uris_deinit();

#endif
