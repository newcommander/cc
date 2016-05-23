#ifndef URI_H
#define URI_H

#include <stdint.h>
#include <pthread.h>

#include "list.h"

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
} Uri;

void init_uri_list();
Uri* get_uri(char *url);
void release_uri(Uri *uri);
Uri* alloc_uri(char *url);
// return 0: success, -1: failed
int free_uri(Uri *uri, int force);

#endif
