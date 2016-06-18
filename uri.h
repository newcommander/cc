#ifndef URI_H
#define URI_H

#include <stdint.h>
#include <pthread.h>

#include "list.h"

typedef void (*frame_opreation)(void *arg);

typedef struct {
    struct list_head list;
    pthread_mutex_t ref_mutex;
    int ref_counter;
    char *url;
#define URI_IDLE 0
#define URI_BUSY 1
#define URI_IN_FREE 2
    int status;
    uint32_t ssrc;
    frame_opreation frame_opt;
} Uri;

void init_uri_list();
Uri* get_uri(char *url);
void release_uri(Uri *uri);
void alloc_uri(char *url, frame_opreation frame_opt);
void add_uris(char *base_url);
void del_uris();

#endif
