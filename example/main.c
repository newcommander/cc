#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "../ccstream/common.h"
#include "../ccstream/ccstream.h"

extern int sampling(void *_frame, int screen_h, int screen_w, void *arg);
extern int lala(void *_frame, int screen_h, int screen_w, void *arg);

struct uri_entry entrys[] = {
    { "example", "trackID=1", 600, 640, 30, NULL, sampling },
    { "lala", "trackID=1", 600, 400, 30, NULL, lala },
    { NULL, NULL, 0, 0, 0, NULL, NULL }
};

unsigned char show_data[640 * 640];

int main(int argc, char **argv)
{
    struct stream_source stream_src;
    pthread_t t;
    unsigned int i, x;

    stream_src.data = show_data;
    stream_src.dim[0] = 640;
    stream_src.dim[1] = 640;
    stream_src.dim[2] = 1;
    stream_src.dim[3] = 1;
    pthread_rwlock_init(&stream_src.sample_lock, NULL);
    entrys[0].data = &stream_src;

    if (pthread_create(&t, NULL, cc_stream, entrys) != 0) {
        printf("%s: Creating cc_stream thread failed", __func__);
        return -1;
    }

    // then modify the memory of show_data, cc_stream will sample it every interval time
    for (i = 0; 1; i++) {
        pthread_rwlock_wrlock(&stream_src.sample_lock);
        memset(show_data, 0, stream_src.dim[0] * stream_src.dim[1] * stream_src.dim[2] * stream_src.dim[3]);
        for (x = 0; x < stream_src.dim[0]; x++)
            show_data[((x + i) % stream_src.dim[1]) * stream_src.dim[0] + x] = 255;
        pthread_rwlock_unlock(&stream_src.sample_lock);
        msleep(50);
    }

    pthread_join(t, NULL);
    pthread_rwlock_destroy(&stream_src.sample_lock);

    return 0;
}
