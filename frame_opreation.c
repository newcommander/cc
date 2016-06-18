#include "rtsp.h"

typedef struct {
    char *title;
    frame_opreation func;
} uri_entry;

static void lala(void *arg)
{
    int height, width, x, y;
    rtsp_session *rs = (rtsp_session*)arg;
    AVFrame *frame;

    frame = rs->frame;
    height = rs->cc->height;
    width = rs->cc->width;

    /* Y */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            frame->data[0][y * frame->linesize[0] + x] = x + y + rs->pts * 3;
        }
    }
    /* Cb and Cr */
    for (y = 0; y < height/2; y++) {
        for (x = 0; x < width/2; x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128 + y + rs->pts * 2;
            frame->data[2][y * frame->linesize[2] + x] = 64 + x + rs->pts * 5;
        }
    }
    frame->pts = rs->pts++;
}

void add_uris(char *base_url)
{
    uri_entry entrys[] = {
        { "lala", lala },
        { NULL, NULL }
    };
    uri_entry *p;
    char url[1024];

    for (p = entrys; p->func; p++) {
        memset(url, 0, 1024);
        if (p->title) {
            if (strlen(base_url) + 1 + strlen(p->title)> 1023)
                continue;
            strncat(url, base_url, strlen(base_url));
            strncat(url, "/", 1);
            strncat(url, p->title, strlen(p->title));
        } else {
            if (strlen(base_url)> 1023)
                continue;
            strncat(url, base_url, strlen(base_url));
        }
        alloc_uri(url, p->func);
    }
}

