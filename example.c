#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "ccstream.h"

unsigned char show_data[640 * 320];
struct stream_arg g_show;

#include "sample_functions.c"

int main(int argc, char **argv)
{
    struct stream_source stream_src;
    pthread_t t;
    unsigned int i, x;

    struct uri_entry entrys[] = {
        { "sample", "trackID=1", 600, 645, 25, NULL, sampling, &sampling_lock },
        { "lala", "trackID=1", 600, 400, 25, NULL, lala, &lala_lock },
        { NULL, NULL, 0, 0, 0, NULL, NULL, NULL }
    };

    stream_src.data = show_data;
    stream_src.dim[0] = 640;
    stream_src.dim[1] = 320;
    stream_src.dim[2] = 1;
    stream_src.dim[3] = 1;
    entrys[0].source = &stream_src;

    if (pthread_create(&t, NULL, cc_stream, entrys) != 0) {
        printf("%s: Creating cc_stream thread failed", __func__);
        return -1;
    }

    // then modify the memory of show_data, cc_stream will sample it every interval time
    for (i = 0; 1; i++) {
        pthread_rwlock_wrlock(&sampling_lock);
        memset(show_data, 0, g_show.dim[0] * g_show.dim[1] * g_show.dim[2] * g_show.dim[3]);
        for (x = 0; x < g_show.dim[0]; x++)
            show_data[((x + i) % g_show.dim[1]) * g_show.dim[0] + x] = 255;
        pthread_rwlock_unlock(&sampling_lock);
        msleep(50);
    }

    pthread_join(t, NULL);
    return 0;
}
