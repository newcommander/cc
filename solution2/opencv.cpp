#define __STDC_CONSTANT_MACROS

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "../ccstream/ccstream.h"
#include "../ccstream/common.h"
#include "../font.h"
}

#define SCREEN_W 600
#define SCREEN_H 400

#define PI 3.14159

static int opencv_sampling(void *_frame, int screen_h, int screen_w, void *arg);
static struct uri_entry entrys[] = {
    { "opencv", "trackID=1", SCREEN_W, SCREEN_H, 30, NULL, opencv_sampling },
    { NULL, NULL, 0, 0, 0, NULL, NULL }
};

static struct SwsContext *sws_ctx = NULL;
static uint8_t *src_data[4];
static int src_linesize[4];
static uint8_t *dst_data[4];
static int dst_linesize[4];

static cv::Mat background;

static pthread_mutex_t motivate_lock = PTHREAD_MUTEX_INITIALIZER;
static std::vector<std::pair<int, int> > motivate_pool;

static void draw_info(cv::Mat &image)
{
    struct char_info *ci = NULL;
    cv::Mat font_mask, sub_font_mask;
    cv::Mat font_mask_array[3];
    cv::Mat font_mask_3c, char_area;
    cv::Mat font_mask_3c_16u, de_font_mask_3c_16u, char_plane_16u, char_area_16u;
    cv::Mat filte_out, filte_out_16u;
//    cv::Scalar text_color = cv::Scalar(173, 121, 54);
    cv::Scalar text_color = cv::Scalar(64, 21, 54);
    int origin_x, origin_y;
    int mask_row_start, mask_row_end, mask_col_start, mask_col_end;
    int area_row_start, area_row_end, area_col_start, area_col_end;
    int height, width;
    unsigned int i;
    char info[] = "space:";

    origin_x = 0;
    origin_y = 20;
    for (i = 0; i < strlen(info); i++) {
        get_char_info(info[i], &ci);
        if (!ci)
            continue;
        if ((origin_x >= image.cols) || (origin_y >= image.rows))
            continue;

        mask_row_start = origin_y > ci->map_top ? 0 : ci->map_top - origin_y;
        mask_row_end = (int)(origin_y + ci->height - ci->map_top) > image.rows ? image.rows - 1 - origin_y + ci->map_top : ci->height - 1;
        mask_col_start = 0;
        mask_col_end = (origin_x + ci->advance) > (image.cols - 1) ? ci->width - 1 - (origin_x + ci->advance - image.cols) : ci->width - 1;
        area_row_start = origin_y - (ci->map_top - mask_row_start);
        area_row_end = area_row_start + (mask_row_end - mask_row_start);
        area_col_start = origin_x;
        area_col_end = area_col_start + (mask_col_end - mask_col_start);

        height = mask_row_end - mask_row_start + 1;
        width = mask_col_end - mask_col_start + 1;
        if ((height <= 1) || (width <= 1))
            continue;

        font_mask = cv::Mat(ci->height, ci->width, CV_8UC1, ci->map);
        sub_font_mask = font_mask(cv::Range(mask_row_start, mask_row_end + 1), cv::Range(mask_col_start, mask_col_end + 1));
        font_mask_array[0] = sub_font_mask;
        font_mask_array[1] = sub_font_mask;
        font_mask_array[2] = sub_font_mask;
        cv::merge(font_mask_array, 3, font_mask_3c);
        char_area = image(cv::Range(area_row_start, area_row_end + 1), cv::Range(area_col_start, area_col_end + 1));

        char_plane_16u = cv::Mat(height, width, CV_16UC3, text_color);
        char_area.convertTo(char_area_16u, CV_16UC3);

        font_mask_3c.convertTo(font_mask_3c_16u, CV_16UC3);
        de_font_mask_3c_16u = cv::Scalar(255, 255, 255) - font_mask_3c_16u;

        filte_out_16u = (char_plane_16u.mul(font_mask_3c_16u) + char_area_16u.mul(de_font_mask_3c_16u)) / 255;
        filte_out_16u.convertTo(filte_out, CV_8UC3);

        filte_out.copyTo(char_area);

        origin_x += ci->advance;
    }
}

static void reset_background(cv::Mat &image)
{
    int i;
    cv::Scalar color(147, 63, 4);

    background = cv::Mat::eye(image.rows, image.cols, image.type());

    for (i = 0; i < background.rows; i++)
        cv::line(background, cv::Point(0, background.rows - 1 - i), cv::Point(background.cols, background.rows - 1 - i),
                color + cv::Scalar(50, 90, 90) * tanh((float)i / background.rows * PI));
}

static void draw_space(cv::Mat &image)
{
    int x, y;
    unsigned int i;
    pthread_mutex_lock(&motivate_lock);
    for (i = 0; i < motivate_pool.size(); i++) {
        x = motivate_pool[i].first;
        y = motivate_pool[i].second;
        image.at<cv::Vec3b>(y+1, x+1) = cv::Vec3b(0, 0, 255);
        image.at<cv::Vec3b>(y+1, x) = cv::Vec3b(0, 0, 255);
        image.at<cv::Vec3b>(y+1, x-1) = cv::Vec3b(0, 0, 255);
        image.at<cv::Vec3b>(y, x+1) = cv::Vec3b(0, 0, 255);
        image.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 255);
        image.at<cv::Vec3b>(y, x-1) = cv::Vec3b(0, 0, 255);
        image.at<cv::Vec3b>(y-1, x+1) = cv::Vec3b(0, 0, 255);
        image.at<cv::Vec3b>(y-1, x) = cv::Vec3b(0, 0, 255);
        image.at<cv::Vec3b>(y-1, x-1) = cv::Vec3b(0, 0, 255);
    }
    motivate_pool.clear();
    pthread_mutex_unlock(&motivate_lock);
}

void motivate_position(int x, int y)
{
    pthread_mutex_lock(&motivate_lock);
    motivate_pool.push_back(std::make_pair<int, int>(x, y));
    pthread_mutex_unlock(&motivate_lock);
}

static void push_rand_pos()
{
    cv::RNG rng(rand());
    motivate_position(rng.uniform(0, SCREEN_W), rng.uniform(0, SCREEN_H));
}

static void opencv_draw(cv::Mat &image)
{
    background.copyTo(image);

    push_rand_pos();
    draw_space(image);

    draw_info(image);
}

static int opencv_sampling(void *_frame, int screen_h, int screen_w, void *arg)
{
    AVFrame *frame = (AVFrame*)_frame;
    struct stream_source *stream_src = (struct stream_source*)arg;
    cv::Mat *image = NULL;
    int object_w, object_h;
    //int channle, num;
    int width, height, y;

    if (!frame || !stream_src || !stream_src->data) {
        printf("%s: Invalid parameter\n", __func__);
        return -1;
    }

    image = (cv::Mat*)stream_src->data;
    object_w = stream_src->dim[0];
    object_h = stream_src->dim[1];
    //channle = stream_src->dim[2];
    //num = stream_src->dim[3];

    pthread_rwlock_rdlock(&stream_src->sample_lock);
    for (y = 0; y < image->rows; y++)
        memcpy(src_data[0] + y * src_linesize[0], image->data + y * image->step[0], image->step[0]);
    pthread_rwlock_unlock(&stream_src->sample_lock);

    sws_scale(sws_ctx, (const uint8_t * const*)src_data, src_linesize, 0, image->rows, dst_data, dst_linesize);

    width  = screen_w > object_w ? object_w : screen_w;
    height = screen_h > object_h ? object_h : screen_h;
    av_image_copy(frame->data, frame->linesize, (const uint8_t **)dst_data, dst_linesize, AV_PIX_FMT_YUV420P, width, height);

    return 0;
}

int main(int argc, char **argv)
{
    struct stream_source stream_src;
    pthread_t t;
    char font_file[] = "font.ttf";
    int ret = -1;

    cv::Mat image = cv::Mat::eye(SCREEN_H, SCREEN_W, CV_8UC3);

    if (av_image_alloc(src_data, src_linesize, image.cols, image.rows, AV_PIX_FMT_BGR24, 16) < 0) {
        printf("alloc src image failed\n");
        return ret;
    }
    if (av_image_alloc(dst_data, dst_linesize, image.cols, image.rows, AV_PIX_FMT_YUV420P, 16) < 0) {
        printf("alloc dst image failed\n");
        goto alloc_dst_image_failed;
    }

    sws_ctx = sws_alloc_context();
    if (!sws_ctx) {
        printf("sws_alloc_context failed\n");
        goto alloc_sws_contex_failed;
    }

    av_opt_set_int(sws_ctx, "sws_flag", SWS_BICUBLIN|SWS_PRINT_INFO, 0);
    av_opt_set_int(sws_ctx, "srcw", image.cols, 0);
    av_opt_set_int(sws_ctx, "srch", image.rows, 0);
    av_opt_set_int(sws_ctx, "dstw", image.cols, 0);
    av_opt_set_int(sws_ctx, "dsth", image.rows, 0);
    av_opt_set_int(sws_ctx, "src_format", AV_PIX_FMT_BGR24, 0);
    av_opt_set_int(sws_ctx, "dst_format", AV_PIX_FMT_YUV420P, 0);
    av_opt_set_int(sws_ctx, "src_range", 1, 0);
    av_opt_set_int(sws_ctx, "dst_range", 1, 0);

    if (sws_init_context(sws_ctx, NULL, NULL) < 0) {
        printf("sws_init_context failed\n");
        goto init_sws_contex_failed;
    }

    stream_src.data = &image;
    stream_src.dim[0] = image.cols;
    stream_src.dim[1] = image.rows;
    stream_src.dim[2] = 1;
    stream_src.dim[3] = 1;
    pthread_rwlock_init(&stream_src.sample_lock, NULL);
    entrys[0].data = &stream_src;

    if (pthread_create(&t, NULL, cc_stream, entrys) != 0) {
        printf("%s: Creating cc_stream thread failed\n", __func__);
        goto create_cc_stream_thread_failed;
    }

    if (font_init(font_file) < 0) {
        printf("%s: Init font library failed.\n", __func__);
        goto create_cc_stream_thread_failed;
    }

    motivate_pool.clear();

    reset_background(image);
    while (1) {
        pthread_rwlock_wrlock(&stream_src.sample_lock);
        opencv_draw(image);
        pthread_rwlock_unlock(&stream_src.sample_lock);
        msleep(10);
    }
    ret = 0;

    pthread_join(t, NULL);

    font_deinit();
create_cc_stream_thread_failed:
    pthread_rwlock_destroy(&stream_src.sample_lock);
init_sws_contex_failed:
    sws_freeContext(sws_ctx);
alloc_sws_contex_failed:
    av_freep(&dst_data[0]);
alloc_dst_image_failed:
    av_freep(&src_data[0]);

    return ret;
}
