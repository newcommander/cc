#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "ccstream.h"

unsigned char show_data[64 * 64];
struct stream_arg g_show;

#include "sample_functions.c"

int main(int argc, char **argv)
{
    pthread_t t;

    g_show.data = show_data;
    g_show.dim[0] = 64;
    g_show.dim[1] = 64;
    g_show.dim[2] = 1;
    g_show.dim[3] = 1;
    memset(show_data, 100, g_show.dim[0] * g_show.dim[1] * g_show.dim[2] * g_show.dim[3]);

    // new thread's arg is the name(char*) of NIC which listenning RTSP request, default eth0
    if (pthread_create(&t, NULL, cc_stream, entrys) != 0) {
        printf("%s: Creating cc_stream thread failed", __func__);
        return -1;
    }

    // then modify the memory of show_data, cc_stream will sample it every interval time

    pthread_join(t, NULL);
    return 0;
}
