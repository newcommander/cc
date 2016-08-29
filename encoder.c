#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "common.h"
#include "session.h"
#include "uri.h"

static int encode_frame(AVCodecContext *cc, AVFrame *frame, unsigned char *out_buf, int *out_buf_len)
{
    int ret = 0, got_output = 0;
    char errbuf[1024];
    AVPacket pkt;

    if (!cc || !frame || !out_buf || !out_buf_len) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    memset(errbuf, 0, sizeof(errbuf));
    av_init_packet(&pkt);
    pkt.data = out_buf;
    pkt.size = PACKET_BUFFER_SIZE;

    /* encode the image */
    ret = avcodec_encode_video2(cc, &pkt, frame, &got_output);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("%s: Error encoding frame [first]: %s\n", __func__, errbuf);
        return -1;
    }
    if (!got_output) {
        ret = avcodec_encode_video2(cc, &pkt, NULL, &got_output);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            printf("%s: Error encoding frame [second]: %s\n", __func__, errbuf);
            return -1;
        }
    }

    if (got_output) {
        if (pkt.size > PACKET_BUFFER_SIZE) {
            printf("%s: Too large packet, drop it\n", __func__);
            return -1;
        }
        //memcpy(out_buf, pkt.data, pkt.size);
        *out_buf_len = pkt.size;
        av_packet_unref(&pkt);
    } else {
        printf("%s: Not got a packet\n", __func__);
        return -1;
    }

    return 0;
}

int sample_frame(struct session *se, unsigned char *out_buf, int *out_buf_len)
{
    if (!se || !se->uri || !se->uri->sample_func) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    se->frame->pts = se->pts;
    if (se->uri->sample_func(se->frame, se->uri->width, se->uri->height) < 0) {
        printf("%s: sampling frame failed\n", __func__);
        return -1;
    }
    se->pts++;

    //TODO: frame filters...

    if (encode_frame(se->cc, se->frame, out_buf, out_buf_len) < 0) {
        printf("%s: Encoding frame failed\n", __func__);
        return -1;
    }

    return 0;
}

void register_encoders()
{
    avcodec_register_all();
}

int encoder_init(struct session *se)
{
    AVCodec *codec = NULL;
    int codec_id;
    int ret = 0;
    char errbuf[1024];

    if (!se || !se->uri || !strlen(se->encoder_name)) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    memset(errbuf, 0, sizeof(errbuf));

    if (!strncmp(se->encoder_name, "mpeg4", 5))
        codec_id = AV_CODEC_ID_MPEG4;
    else if (!strncmp(se->encoder_name, "h264", 4))
        codec_id = AV_CODEC_ID_H264;
    else
        codec_id = AV_CODEC_ID_H264;

    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        printf("%s: Codec not found\n", __func__);
        goto failed;
    }

    se->cc = avcodec_alloc_context3(codec);
    if (!se->cc) {
        printf("%s: Could not allocate video codec context\n", __func__);
        goto failed;
    }

    /* frames per second */
    se->cc->time_base = (AVRational){1, se->uri->framerate};
    se->cc->bit_rate = 400000;
    /* resolution must be a multiple of two */
    se->cc->width = se->uri->width;
    se->cc->height = se->uri->height;
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    se->cc->gop_size = 10;
    se->cc->max_b_frames = 1;
    se->cc->pix_fmt = AV_PIX_FMT_YUV420P;

    se->cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (codec_id == AV_CODEC_ID_H264) {
        av_opt_set(se->cc->priv_data, "preset", "slow", 0);
        av_opt_set(se->cc->priv_data, "allow_skip_frames", "enable", 0);
    } else if (codec_id == AV_CODEC_ID_MPEG4) {
        ;
    }

    /* open it */
    if (avcodec_open2(se->cc, codec, NULL) < 0) {
        printf("%s: Could not open codec\n", __func__);
        goto failed;
    }

    if (!se->frame) {
        se->frame = av_frame_alloc();
        if (!se->frame) {
            printf("%s: Could not allocate video frame\n", __func__);
            goto failed;
        }
    }
    se->frame->format = se->cc->pix_fmt;
    se->frame->width  = se->cc->width;
    se->frame->height = se->cc->height;

    if (!se->frame->data[0]) {
        ret = av_image_alloc(se->frame->data, se->frame->linesize, se->frame->width, se->frame->height, se->cc->pix_fmt, 32);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            printf("%s: Could not allocate raw picture buffer: %s\n", __func__, errbuf);
            goto failed;
        }
    }

    return 0;

failed:
    if (se->cc) {
        if (avcodec_is_open(se->cc))
            avcodec_close(se->cc);
        av_free(se->cc);
        se->cc = NULL;
    }
    if (se->frame) {
        if (se->frame->data[0])
            av_freep(&se->frame->data[0]);
        av_frame_free(&se->frame);
        se->frame = NULL;
    }
    return -1;
}

void encoder_deinit(struct session *se)
{
    if (!se) {
        printf("%s: Invalid parameter\n", __func__);
        return;
    }
    if (se->cc) {
        if (avcodec_is_open(se->cc))
            avcodec_close(se->cc);
        av_free(se->cc);
        se->cc = NULL;
    }
    if (se->frame) {
        if (se->frame->data[0])
            av_freep(&se->frame->data[0]);
        av_frame_free(&se->frame);
        se->frame = NULL;
    }
}

static char *data_to_hex(char *buff, const uint8_t *src, int s, int lowercase)
{
    int i;
    static const char hex_table_uc[16] = { '0', '1', '2', '3',
                                           '4', '5', '6', '7',
                                           '8', '9', 'A', 'B',
                                           'C', 'D', 'E', 'F' };
    static const char hex_table_lc[16] = { '0', '1', '2', '3',
                                           '4', '5', '6', '7',
                                           '8', '9', 'a', 'b',
                                           'c', 'd', 'e', 'f' };
    const char *hex_table = lowercase ? hex_table_lc : hex_table_uc;

    for (i = 0; i < s; i++) {
        buff[i * 2]     = hex_table[src[i] >> 4];
        buff[i * 2 + 1] = hex_table[src[i] & 0xF];
    }
    return buff;
}

int get_media_config(struct Uri *uri, char *encoder_name, char *buf, int size)
{
    struct session se;

    memset(&se, 0, sizeof(se));
    snprintf(se.encoder_name, sizeof(se.encoder_name), encoder_name);
    se.uri = uri;
    // open and then close codec, we got sdp string
    if (encoder_init(&se) < 0) {
        printf("%s: Cannot init encoder\n", __func__);
        return -1;
    }
    if (!se.cc) {
        printf("%s: Cannot get codec context\n", __func__);
        encoder_deinit(&se);
        return -1;
    }
    if (!memcmp(se.encoder_name, "mpeg4", 5)) {
        if (size < 9 + se.cc->extradata_size * 2 + 1) {
            printf("%s: The buffer for media config is too small\n", __func__);
            encoder_deinit(&se);
            return -1;
        }
        strncat(buf, "; config=", 9);
        data_to_hex(buf + strlen(buf), se.cc->extradata, se.cc->extradata_size, 0);
    } else if (!memcmp(se.encoder_name, "h264", 4)) {
        char *config = NULL;
        config = av_get_sdp_config(se.cc);
        if (config) {
            if (strlen(config) <= size) {
                memcpy(buf, config, strlen(config));
                av_free(config);
            } else {
                printf("%s: Codec[h264]'s config info too long\n", __func__);
                av_free(config);
                encoder_deinit(&se);
                return -1;
            }
        } else {
            printf("%s: Cannot get config of codec[h264]\n", __func__);
            encoder_deinit(&se);
            return -1;
        }
    }
    encoder_deinit(&se);

    return 0;
}
