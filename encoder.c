#include <string.h>
#include <stdlib.h>
#include <math.h>

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

static int samping_frame(AVCodecContext *cc, AVFrame *frame, unsigned char *data, int *len)
{
    int ret = 0, got_output = 0;
    char errbuf[100];
    AVPacket pkt;

    if (!cc || !frame) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    if (!rs->uri || !rs->uri->frame_opt) {
        printf("%s: No URI present or no frame operation function", __func__);
        return -1;
    }
    memset(errbuf, 0, 100);

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    rs->uri->frame_opt(rs);

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
        memcpy(data, pkt.data, pkt.size);
        *len = pkt.size;
        av_packet_unref(&pkt);
    } else {
        printf("%s: Not got packet\n", __func__);
        return -1;
    }
    return 0;
}

int video_encoding(rtsp_session *rs, unsigned char *data, int *len)
{
    int ret = 0;

    if (!rs) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    if (!rs->cc || !rs->frame) {
        ret = encoder_init(rs);
        if (ret < 0)
            return -1;
    }

    ret = samping_frame(rs, data, len);
    if (ret < 0)
        return -1;

    return 0;
}

int encoder_init(rtsp_session *rs)
{
    AVCodec *codec = NULL;
    int codec_id = AV_CODEC_ID_MPEG4;
    //int codec_id = AV_CODEC_ID_H264;
    int ret = 0;
    char errbuf[100];

    if (!rs) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }
    memset(errbuf, 0, 100);

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

    /* frames per second */
    rs->cc->time_base = (AVRational){1,25};
    rs->cc->bit_rate = 400000;
    /* resolution must be a multiple of two */
    rs->cc->width = 600;
    rs->cc->height = 400;
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    rs->cc->gop_size = 10;
    rs->cc->max_b_frames = 1;
    rs->cc->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec_id == AV_CODEC_ID_H264) {
        av_opt_set(rs->cc->priv_data, "preset", "slow", 0);
        av_opt_set(rs->cc->priv_data, "allow_skip_frames", "enable", 0);
    } else if (codec_id == AV_CODEC_ID_MPEG4)
        rs->cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    /* open it */
    if (avcodec_open2(rs->cc, codec, NULL) < 0) {
        printf("%s: Could not open codec\n", __func__);
        goto failed;
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

    char *config = (char*)calloc(rs->cc->extradata_size * 2 + 1, 1);
    data_to_hex(config, rs->cc->extradata, rs->cc->extradata_size, 0);
    printf("config_size=%d\n", rs->cc->extradata_size);
    printf("config=%s\n", config);
    free(config);

    if (!rs->frame->data[0]) {
        ret = av_image_alloc(rs->frame->data, rs->frame->linesize, rs->cc->width, rs->cc->height, rs->cc->pix_fmt, 32);
        if (ret < 0) {
            av_strerror(ret, errbuf, sizeof(errbuf));
            printf("%s: Could not allocate raw picture buffer: %s\n", __func__, errbuf);
            goto failed;
        }
    }

    return 0;

failed:
    if (rs->cc) {
        if (avcodec_is_open(rs->cc))
            avcodec_close(rs->cc);
        av_free(rs->cc);
        rs->cc = NULL;
    }
    if (rs->frame) {
        if (rs->frame->data[0])
            av_freep(&rs->frame->data[0]);
        av_frame_free(&rs->frame);
        rs->frame = NULL;
    }
    return -1;
}

void encoder_deinit(rtsp_session *rs)
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
