#include <stdio.h>
#include <pthread.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>

#include "uri.h"

static int sampling(void *_frame, pthread_mutex_t *sample_mutex, int screen_w, int screen_h);
static int lala(void *_frame, pthread_mutex_t *sample_mutex, int screen_w, int screen_h);

static pthread_mutex_t sampling_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lala_mutex = PTHREAD_MUTEX_INITIALIZER;

struct uri_entry entrys[] = {
    { "sample", "trackID=1", 600, 800, 25, &sampling_mutex, sampling },
    { "lala", "trackID=1", 600, 400, 25, &lala_mutex, lala },
    { NULL, NULL, 0, 0, 0, NULL }
};

static int sampling(void *_frame, pthread_mutex_t *sample_mutex, int screen_w, int screen_h)
{
    AVFrame *frame = (AVFrame*)_frame;
    uint8_t *data = NULL;
    int object_w, object_h, channle, num;
    int width, high, y;

    if (!frame || screen_w <= 0 || screen_h <= 0) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    data       = g_show.data;
    object_w   = g_show.dim[0];
    object_h   = g_show.dim[1];
    channle    = g_show.dim[2];
    num        = g_show.dim[3];

    // clear screen
    memset(frame->data[0], 0, screen_h * frame->linesize[0]);      // Y
    memset(frame->data[1], 128, screen_h/2 * frame->linesize[1]);  // Cb
    memset(frame->data[2], 128, screen_h/2 * frame->linesize[2]);  // Cr

    high = screen_h > object_h ? object_h : screen_h;
    width = screen_w > object_w ? object_w : screen_w;

    pthread_mutex_lock(sample_mutex);
    for (y = 0; y < high; y++)
        memcpy(frame->data[0] + y * frame->linesize[0], data + y * object_w, width);
    pthread_mutex_unlock(sample_mutex);

    return 0;
}

static int lala(void *_frame, pthread_mutex_t *sample_mutex, int screen_w, int screen_h)
{
    int unit_h, unit_w;
    int num, channle, i, j, x, y, units_in_line;
    AVFrame *frame = (AVFrame*)_frame;
    float *data = (float*)g_show.data;

    unit_w = g_show.dim[1];
    unit_h = g_show.dim[0];
    channle = g_show.dim[2];
    num = g_show.dim[3];
    units_in_line = 9;

    /* Y */
    for (y = 0; y < screen_h; y++) {
        for (x = 0; x < screen_w; x++) {
            frame->data[0][y * frame->linesize[0] + x] = 0;
        }
    }
    /* Cb and Cr */
    for (y = 0; y < screen_h / 2; y++) {
        for (x = 0; x < screen_w / 2; x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128;
            frame->data[2][y * frame->linesize[2] + x] = 128;
        }
    }

    pthread_mutex_lock(sample_mutex);
    for (i = 0; i < num; i++) {
        j = i % units_in_line;
        i = i / units_in_line;
        for (y = 0; y < unit_h; y++) {
            for (x = 0; x < unit_w; x++) {
                frame->data[0][(y + i * unit_h) * frame->linesize[0] + (x + j * unit_w)] = (int)(data[(y + (i * units_in_line + j) * unit_h) * unit_w + (x + j * unit_w)] * 3100);
            }
        }
        i = i * units_in_line + j;
    }
    pthread_mutex_unlock(sample_mutex);

    return 0;
}

