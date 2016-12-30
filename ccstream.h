#ifndef CCSTREAM_H
#define CCSTREAM_H

typedef int (*sample_function)(void *_frame, int screen_h, int screen_w, void *arg);

struct uri_entry {
    char *title;
    char *track;
    int screen_w;
    int screen_h;
    int framerate;
    void *data;
    sample_function sample_func;
};

struct stream_source {
    pthread_rwlock_t sample_lock;
    void *data;
    int dim[4];
};

void *cc_stream(void *arg);

#endif /* CCSTREAM_H */
