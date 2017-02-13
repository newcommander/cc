#define __STDC_CONSTANT_MACROS

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
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
#include "../ccstream.h"
#include "../common.h"
}

#define SCREEN_W 600
#define SCREEN_H 400

#define PI 3.14159

static int opencv_sampling(void *_frame, int screen_h, int screen_w, void *arg);
static struct uri_entry entrys[] = {
    { "opencv", "trackID=1", SCREEN_W, SCREEN_H, 60, NULL, opencv_sampling },
    { NULL, NULL, 0, 0, 0, NULL, NULL }
};

struct SwsContext *sws_ctx = NULL;
static uint8_t *src_data[4];
static int src_linesize[4];
static uint8_t *dst_data[4];
static int dst_linesize[4];

typedef std::vector<std::pair<cv::Point3d, cv::Scalar> > Color_Points;
static Color_Points g_aixs;
static Color_Points g_obj;

static struct color_point_data {
    cv::Point3d coordinate;
    cv::Scalar color;
} aixs_data[] = {
    { cv::Point3d(1.0, 0.0, 0.0), cv::Scalar(0, 0, 255) },
    { cv::Point3d(0.0, 1.0, 0.0), cv::Scalar(0, 255, 0) },
    { cv::Point3d(0.0, 0.0, 1.0), cv::Scalar(255, 0, 0) }
}, obj_data[] = {
    { cv::Point3d(0.0, 1.0, 0.0),      cv::Scalar(0, 0, 255) },
    { cv::Point3d(0.0, 0.0, -1.0),     cv::Scalar(0, 255, 0) },
    { cv::Point3d(0.707, 0.0, 0.225),  cv::Scalar(255, 0, 0) },
    { cv::Point3d(-0.707, 0.0, 0.225), cv::Scalar(0, 128, 128) }
};

static void insert_color_point(Color_Points &obj, cv::Point3d coordinate, cv::Scalar color)
{
    obj.push_back(std::make_pair<cv::Point3d, cv::Scalar>(coordinate, color));
}

static void init_variables(void)
{
    int i;

    for (i = 0; i < 3; i++)
        insert_color_point(g_aixs, aixs_data[i].coordinate, aixs_data[i].color);
    for (i = 0; i < 4; i++)
        insert_color_point(g_obj, obj_data[i].coordinate, obj_data[i].color);
}

static cv::Point3d rotating_on_x_aixs(cv::Point3d &point, double angle)
{
    cv::Point3d new_point;

    new_point.x = point.x;
    new_point.y = point.y * cos(angle) - point.z * sin(angle);
    new_point.z = point.y * sin(angle) + point.z * cos(angle);

    return new_point;
}

static cv::Point3d rotating_on_y_aixs(cv::Point3d &point, double angle)
{
    cv::Point3d new_point;

    new_point.x = point.x * cos(angle) + point.z * sin(angle);
    new_point.y = point.y;
    new_point.z = point.z * cos(angle) - point.x * sin(angle);

    return new_point;
}

static cv::Point3d rotating_on_z_aixs(cv::Point3d &point, double angle)
{
    cv::Point3d new_point;

    new_point.x = point.x * cos(angle) - point.y * sin(angle);
    new_point.y = point.x * sin(angle) + point.y * cos(angle);
    new_point.z = point.z;

    return new_point;
}

static void rotating_object(Color_Points &object, Color_Points &output, bool in_place, double x_angle, double y_angle, double z_angle, std::string rotate_order)
{
    Color_Points *p = NULL;
    unsigned int i, c;

    if (in_place) {
        p = &object;
    } else {
        output = object;
        p = &output;
    }

    for (c = 0; c < rotate_order.length(); c++)
        switch (rotate_order[c]) {
        case 'x':
            if (x_angle != 0)
                for (i = 0; i < p->size(); i++)
                    (*p)[i].first = rotating_on_x_aixs((*p)[i].first, x_angle);
            break;
        case 'y':
            if (y_angle != 0)
                for (i = 0; i < p->size(); i++)
                    (*p)[i].first = rotating_on_y_aixs((*p)[i].first, y_angle);
            break;
        case 'z':
            if (z_angle != 0)
                for (i = 0; i < p->size(); i++)
                    (*p)[i].first = rotating_on_z_aixs((*p)[i].first, z_angle);
        };
}

static void draw_aixs(cv::Mat &image, Color_Points &aixs)
{
    cv::Point origin(image.cols/2, image.rows/2), head;
    int lineType = cv::LINE_AA, thickness = 1, scale = 50;

    if (aixs.size() < 3) {
        printf("%s(): Invalid parameter\n", __func__);
        return;
    }

    head = cv::Point(aixs[0].first.x * scale + origin.x, origin.y - aixs[0].first.y * scale);
    cv::arrowedLine(image, origin, head, aixs[0].second, thickness, lineType);

    head = cv::Point(aixs[1].first.x * scale + origin.x, origin.y - aixs[1].first.y * scale);
    cv::arrowedLine(image, origin, head, aixs[1].second, thickness, lineType);

    head = cv::Point(aixs[2].first.x * scale + origin.x, origin.y - aixs[2].first.y * scale);
    cv::arrowedLine(image, origin, head, aixs[2].second, thickness, lineType);
}

static void draw_obj(cv::Mat &image, Color_Points &obj)
{
    cv::Point origin(image.cols/2, image.rows/2), head1, head2;
    int lineType = cv::LINE_AA, thickness = 1, scale = 100;

    if (obj.size() < 4) {
        printf("%s(): Invalid parameter\n", __func__);
        return;
    }

    head1 = cv::Point(obj[0].first.x * scale + origin.x, origin.y - obj[0].first.y * scale);

    head2 = cv::Point(obj[1].first.x * scale + origin.x, origin.y - obj[1].first.y * scale);
    cv::line(image, head1, head2, cv::Scalar(255, 0, 0), thickness, lineType);

    head2 = cv::Point(obj[2].first.x * scale + origin.x, origin.y - obj[2].first.y * scale);
    cv::line(image, head1, head2, cv::Scalar(0, 255, 0), thickness, lineType);

    head2 = cv::Point(obj[3].first.x * scale + origin.x, origin.y - obj[3].first.y * scale);
    cv::line(image, head1, head2, cv::Scalar(0, 0, 255), thickness, lineType);

    head1 = cv::Point(obj[1].first.x * scale + origin.x, origin.y - obj[1].first.y * scale);

    head2 = cv::Point(obj[2].first.x * scale + origin.x, origin.y - obj[2].first.y * scale);
    cv::line(image, head1, head2, cv::Scalar(0, 255, 255), thickness, lineType);

    head2 = cv::Point(obj[3].first.x * scale + origin.x, origin.y - obj[3].first.y * scale);
    cv::line(image, head1, head2, cv::Scalar(255, 0, 255), thickness, lineType);

    head1 = cv::Point(obj[2].first.x * scale + origin.x, origin.y - obj[2].first.y * scale);
    head2 = cv::Point(obj[3].first.x * scale + origin.x, origin.y - obj[3].first.y * scale);
    cv::line(image, head1, head2, cv::Scalar(255, 255, 0), thickness, lineType);
}

static void opencv_draw(cv::Mat &image)
{
    std::stringstream s;
    Color_Points output;
    cv::String text;
    cv::Size text_size;
    cv::Scalar text_color = cv::Scalar(173, 121, 54);
    int text_font = cv::FONT_HERSHEY_DUPLEX;
    double text_scale = 0.5;
    int text_thickness = 1;

    cv::rectangle(image, cv::Point(0, 0), cv::Point(image.cols, image.rows), cv::Scalar(86, 60, 27), -1);

    rotating_object(g_aixs, output, true, PI/100, -PI/100, 0.0, "yxz");
    draw_aixs(image, g_aixs);

    rotating_object(g_obj, output, true, PI/100, -PI/100, 0.0, "yxz");
    draw_obj(image, g_obj);

    s << "aixs_x=" << g_aixs[0].first;
    text = s.str();
    text_size = cv::getTextSize(text, text_font, text_scale, text_thickness, NULL);
    cv::putText(image, text, cv::Point(10, 20), text_font, text_scale, text_color, text_thickness, cv::LINE_AA, false);

    s.str("");
    s << "aixs_y=" << g_aixs[1].first;
    text = s.str();
    text_size = cv::getTextSize(text, text_font, text_scale, text_thickness, NULL);
    cv::putText(image, text, cv::Point(10, 20 + (10 + text_size.height)), text_font, text_scale, text_color, text_thickness, cv::LINE_AA, false);

    s.str("");
    s << "aixs_z=" << g_aixs[2].first;
    text = s.str();
    cv::putText(image, text, cv::Point(10, 20 + (10 + text_size.height) * 2), text_font, text_scale, text_color, text_thickness, cv::LINE_AA, false);
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

    init_variables();
    while (1) {
        pthread_rwlock_wrlock(&stream_src.sample_lock);
        image = cv::Mat::zeros(image.rows, image.cols, CV_8UC3);
        opencv_draw(image);
        pthread_rwlock_unlock(&stream_src.sample_lock);
        msleep(100);
    }
    ret = 0;

    pthread_join(t, NULL);

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
