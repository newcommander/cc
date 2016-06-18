#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "rtsp.h"

static int samping_frame(rtsp_session *rs, unsigned char *data, int *len)
{
    int ret = 0, got_output = 0;
    AVPacket pkt;

    if (!rs || !rs->cc || !rs->frame) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    rs->uri->frame_opt(rs);

    /* encode the image */
    ret = avcodec_encode_video2(rs->cc, &pkt, rs->frame, &got_output);
    if (ret < 0) {
        printf("%s: Error encoding frame [first]\n", __func__);
        return -1;
    }
    if (!got_output) {
        ret = avcodec_encode_video2(rs->cc, &pkt, NULL, &got_output);
        if (ret < 0) {
            printf("%s: Error encoding frame [second]\n", __func__);
            return -1;
        }
    }

    if (got_output) {
        if (pkt.size > PACKET_BUFFER_SIZE) {
            printf("%s: Too large packet, drop it\n", __func__);
            return -1;
        }
        memcpy(data, pkt.data, pkt.size);
        *len = pkt.size;
        av_packet_unref(&pkt);
    } else {
        printf("%s: Not got packet\n", __func__);
        return -1;
    }
    return 0;
}

int encoding_init(rtsp_session *rs)
{
    AVCodec *codec;
    int codec_id = AV_CODEC_ID_MPEG4;
    int ret = 0;

    if (!rs) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    avcodec_register_all();

    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        printf("%s: Codec not found\n", __func__);
        goto failed;
    }

    if (!rs->cc) {
        rs->cc = avcodec_alloc_context3(codec);
        if (!rs->cc) {
            printf("%s: Could not allocate video codec context\n", __func__);
            goto failed;
        }
    }

    /* put sample parameters */
    rs->cc->bit_rate = 400000;
    /* resolution must be a multiple of two */
    rs->cc->width = 469;
    rs->cc->height = 288;
    /* frames per second */
    rs->cc->time_base = (AVRational){1,25};
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    rs->cc->gop_size = 10;
    rs->cc->max_b_frames = 1;
    rs->cc->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec_id == AV_CODEC_ID_H264)
        av_opt_set(rs->cc->priv_data, "preset", "slow", 0);

    /* open it */
    if (!avcodec_is_open(rs->cc)) {
        if (avcodec_open2(rs->cc, codec, NULL) < 0) {
            printf("%s: Could not open codec\n", __func__);
            goto failed;
        }
    }

    if (!rs->frame) {
        rs->frame = av_frame_alloc();
        if (!rs->frame) {
            printf("%s: Could not allocate video frame\n", __func__);
            goto failed;
        }
    }
    rs->frame->format = rs->cc->pix_fmt;
    rs->frame->width  = rs->cc->width;
    rs->frame->height = rs->cc->height;

    if (!rs->frame->data[0]) {
        ret = av_image_alloc(rs->frame->data, rs->frame->linesize, rs->cc->width, rs->cc->height, rs->cc->pix_fmt, 32);
        if (ret < 0) {
            printf("%s: Could not allocate raw picture buffer\n", __func__);
            goto failed;
        }
    }

    return 0;

failed:
    if (avcodec_is_open(rs->cc))
        avcodec_close(rs->cc);
    if (rs->cc) {
        av_free(rs->cc);
        rs->cc = NULL;
    }
    if (rs->frame->data[0])
        av_freep(&rs->frame->data[0]);
    if (rs->frame) {
        av_frame_free(&rs->frame);
        rs->frame = NULL;
    }
    return -1;
}

void encoding_deinit(rtsp_session *rs)
{
    if (!rs) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }
    if (avcodec_is_open(rs->cc))
        avcodec_close(rs->cc);
    if (rs->cc) {
        av_free(rs->cc);
        rs->cc = NULL;
    }
    if (rs->frame->data[0])
        av_freep(&rs->frame->data[0]);
    if (rs->frame) {
        av_frame_free(&rs->frame);
        rs->frame = NULL;
    }
}

void video_encode(rtsp_session *rs, unsigned char *data, int *len)
{
    int ret = 0;

    if (!rs) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }

    if (!rs->cc || !rs->frame) {
        ret = encoding_init(rs);
        if (ret < 0)
            return;
    }

    ret = samping_frame(rs, data, len);
    if (ret < 0)
        return;
}

