#include <stdio.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>

static int sampling(void *_frame, int screen_h, int screen_w, void *arg)
{
    AVFrame *frame = (AVFrame*)_frame;
    struct stream_source *src = (struct stream_source*)arg;
    int object_w, object_h, channle, num;
    int width, height, y;

    if (!frame || !src || !src->data) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    object_w = src->dim[0];
    object_h = src->dim[1];
    channle = src->dim[2];
    num = src->dim[3];

    // clear screen
    //memset(frame->data[0], 0, screen_h * frame->linesize[0]);      // Y
    //memset(frame->data[1], 128, screen_h/2 * frame->linesize[1]);  // Cb
    //memset(frame->data[2], 128, screen_h/2 * frame->linesize[2]);  // Cr

    height = screen_h > object_h ? object_h : screen_h;
    width  = screen_w > object_w ? object_w : screen_w;

    pthread_rwlock_rdlock(&src->sample_lock);
    for (y = 0; y < height; y++)
        memcpy(frame->data[0] + y * frame->linesize[0], src->data + y * object_w, width);
    pthread_rwlock_unlock(&src->sample_lock);

    return 0;
}

static int lala(void *_frame, int screen_h, int screen_w, void *arg)
{
    AVFrame *frame = (AVFrame*)_frame;
    struct stream_source *src = (struct stream_source*)arg;
    int object_w, object_h, channle, num;
    int width, height, y;

    if (!frame || !src || !src->data) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    object_w = src->dim[0];
    object_h = src->dim[1];
    channle = src->dim[2];
    num = src->dim[3];

    // clear screen
    //memset(frame->data[0], 0, screen_h * frame->linesize[0]);      // Y
    //memset(frame->data[1], 128, screen_h/2 * frame->linesize[1]);  // Cb
    //memset(frame->data[2], 128, screen_h/2 * frame->linesize[2]);  // Cr

    height = screen_h > object_h ? object_h : screen_h;
    width  = screen_w > object_w ? object_w : screen_w;

    pthread_rwlock_rdlock(&src->sample_lock);
    for (y = 0; y < height; y++)
        memcpy(frame->data[0] + y * frame->linesize[0], src->data + y * object_w, width);
    pthread_rwlock_unlock(&src->sample_lock);

    return 0;
}

