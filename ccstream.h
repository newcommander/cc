#ifndef CCSTREAM_H
#define CCSTREAM_H

struct stream_arg {
    void *data;
    int dim[4];
};

void *cc_stream(void *arg);

#endif /* CCSTREAM_H */
