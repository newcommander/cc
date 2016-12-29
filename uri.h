#ifndef URI_H
#define URI_H

#include <pthread.h>
#include <stdint.h>

#include "list.h"

typedef int (*sample_function)(void *_frame, void *arg);

struct uri_entry {
    char *title;
    char *track;
    int screen_w;
    int screen_h;
    int framerate;
    void *source;
    sample_function sample_func;
    pthread_rwlock_t *sample_lock;
};

struct Uri {
    struct list_head list;
    struct list_head user_list;
    pthread_mutex_t user_list_mutex;
    int user_num;
    uint32_t ssrc;
    char *sdp_str_mpeg4;
    char *sdp_str_h264;
    char *url;
    struct uri_entry *entry;
};

#include "session.h"

int add_uri(struct uri_entry *ue);
void del_uri(struct Uri *uri);
struct Uri* find_uri(char *url);
int ref_uri(struct session *se);
int unref_uri(struct session *se);
void uris_init(struct uri_entry *entrys);
void uris_deinit();

#endif /* URI_H */
