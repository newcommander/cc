#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "ccstream.h"

unsigned char show_data[64 * 64];

int main(int argc, char **argv)
{
    pthread_t t;

    g_show.data = show_data;
    g_show.dim[0] = 64;
    g_show.dim[1] = 64;
    g_show.dim[2] = 0;
    g_show.dim[3] = 0;
    memset(show_data, 100, 64 * 64);

    // new thread's arg is the name(char*) of NIC which listenning RTSP request, default eth0
    if (pthread_create(&t, NULL, cc_stream, NULL) != 0) {
        printf("%s: Creating cc_stream thread failed", __func__);
        return -1;
    }

    // then modify the memory of show_data, cc_stream will sample it every interval time

    pthread_join(t, NULL);
    return 0;
}
