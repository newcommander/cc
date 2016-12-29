#include <stdio.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>

#include "uri.h"

static pthread_rwlock_t sampling_lock;
static pthread_rwlock_t lala_lock;

static int sampling(void *_frame, void *arg)
{
    AVFrame *frame = (AVFrame*)_frame;
    struct uri_entry *entry = (struct uri_entry*)arg;
    struct stream_src *source = NULL;
    uint8_t *data = NULL;
    int object_w, object_h, channle, num;
    int width, height, y;

    if (!frame || !entry || !entry->source || entry->screen_w <= 0 || entry->screen_h <= 0) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    source = (struct stream_src*)entry->source;
    data = (uint8_t*)source->data;
    object_w = source->dim[0];
    object_h = source->dim[1];
    channle = source->dim[2];
    num = source->dim[3];

    // clear screen
    //memset(frame->data[0], 0, screen_h * frame->linesize[0]);      // Y
    //memset(frame->data[1], 128, screen_h/2 * frame->linesize[1]);  // Cb
    //memset(frame->data[2], 128, screen_h/2 * frame->linesize[2]);  // Cr

    height = entry->screen_h > object_h ? object_h : entry->screen_h;
    width = entry->screen_w > object_w ? object_w : entry->screen_w;

    pthread_rwlock_rdlock(entry->sample_lock);
    for (y = 0; y < height; y++)
        memcpy(frame->data[0] + y * frame->linesize[0], data + y * object_w, width);
    pthread_rwlock_unlock(entry->sample_lock);

    return 0;
}

static int lala(void *_frame, void *arg)
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

    pthread_rwlock_rdlock(sample_lock);
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
    pthread_rwlock_unlock(sample_lock);

    return 0;
}

