#ifndef CCSTREAM_H
#define CCSTREAM_H

struct stream_arg {
    void *data;
    int dim[4];
};

struct stream_arg g_show;

void *cc_stream(void *arg);

#endif /* CCSTREAM_H */
