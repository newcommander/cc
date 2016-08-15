#include "ccstream.h"
#include "common.h"
#include "rtsp.h"

static void sample_function(void *arg);
static void lala(void *arg);

uri_entry entrys[] = {
    { "sample", sample_function },
    { "lala", lala },
    { NULL, NULL }
};

struct stream_arg g_show;

static void sample_function(void *arg)
{
    rtsp_session *rs = (rtsp_session*)arg;
    AVFrame *frame;
    int screen_w, screen_h, unit_w, unit_h, channle, num;
    int x, y;
    uint8_t *data = NULL;

    if (!rs)
        return;

    data     = g_show.data;
    unit_w   = g_show.dim[0];
    unit_h   = g_show.dim[1];
    channle  = g_show.dim[2];
    num      = g_show.dim[3];
    frame    = rs->frame;
    screen_w = rs->cc->width;
    screen_h = rs->cc->height;

    if (unit_w > screen_w || unit_h > screen_h)
        return;

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

    for (y = 0; y < unit_h; y++) {
        for (x = 0; x < unit_w; x++) {
            //frame->data[0][y * frame->linesize[0] + x] = data[unit_w * y + x];
            frame->data[0][y * frame->linesize[0] + x] = rs->pts % 255;
        }
    }
    frame->pts = rs->pts++;
}

static void lala(void *arg)
{
    int screen_h, screen_w, unit_h, unit_w;
    int num, channle, i, j, x, y, units_in_line;
    rtsp_session *rs = (rtsp_session*)arg;
    AVFrame *frame;
    float *data = (float*)g_show.data;

    frame = rs->frame;
    screen_h = rs->cc->height;
    screen_w = rs->cc->width;
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
            frame->data[1][y * frame->linesize[1] + x] = 0;
            frame->data[2][y * frame->linesize[2] + x] = 0;
        }
    }
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
    frame->pts = rs->pts++;
}

