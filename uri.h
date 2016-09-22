#ifndef URI_H
#define URI_H

#include <pthread.h>
#include <stdint.h>

#include "list.h"

typedef int (*sample_function)(void *_frame, int screen_w, int screen_h);

struct Uri {
    struct list_head list;
    struct list_head user_list;
    pthread_mutex_t user_list_mutex;
    int user_num;
    uint32_t ssrc;
    char *sdp_str_mpeg4;
    char *sdp_str_h264;
    char *url;
    char *track;
    int width;
    int height;
    int framerate;
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

#include "session.h"

int add_uri(struct uri_entry *ue);
void del_uri(struct Uri *uri);
struct Uri* find_uri(char *url);
int ref_uri(struct session *se);
int unref_uri(struct session *se);
void uris_init();
void uris_deinit();

#endif /* URI_H */
