#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "ccstream.h"

unsigned char show_data[640 * 640];
struct stream_arg g_show;

#include "sample_functions.c"

int main(int argc, char **argv)
{
    pthread_t t;
    unsigned int i, x;

    g_show.data = show_data;
    g_show.dim[0] = 640;
    g_show.dim[1] = 640;
    g_show.dim[2] = 1;
    g_show.dim[3] = 1;

    // new thread's arg is the name(char*) of NIC which listenning RTSP request, default eth0
    if (pthread_create(&t, NULL, cc_stream, entrys) != 0) {
        printf("%s: Creating cc_stream thread failed", __func__);
        return -1;
    }

    // then modify the memory of show_data, cc_stream will sample it every interval time
    for (i = 0; 1; i++) {
        pthread_mutex_lock(&sampling_mutex);
        memset(show_data, 0, g_show.dim[0] * g_show.dim[1] * g_show.dim[2] * g_show.dim[3]);
        for (x = 0; x < g_show.dim[0]; x++)
            show_data[((x + i) % g_show.dim[1]) * g_show.dim[0] + x] = 255;
        pthread_mutex_unlock(&sampling_mutex);
        msleep(50);
    }

    pthread_join(t, NULL);
    return 0;
}
