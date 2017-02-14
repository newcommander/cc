#define __STDC_CONSTANT_MACROS

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <iostream>
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

static Color_Points world_aixs;
static Color_Points camera_aixs;
static cv::Point3d camera_pos;

static Color_Points g_obj;

static struct color_point_data {
    cv::Point3d coordinate;
    cv::Scalar color;
} world_aixs_data[] = {
    { cv::Point3d(1.0, 0.0, 0.0), cv::Scalar(0,     0, 255) },
    { cv::Point3d(0.0, 1.0, 0.0), cv::Scalar(0,   255,   0) },
    { cv::Point3d(0.0, 0.0, 1.0), cv::Scalar(255,   0,   0) }
}, g_obj_data[] = {
    { cv::Point3d(   0.0, 1.0,   0.0), cv::Scalar(0,     0, 255) },
    { cv::Point3d(   0.0, 0.0,  -1.0), cv::Scalar(0,   255,   0) },
    { cv::Point3d( 0.707, 0.0, 0.225), cv::Scalar(255,   0,   0) },
    { cv::Point3d(-0.707, 0.0, 0.225), cv::Scalar(0,   128, 128) }
};

static void insert_color_point(Color_Points &obj, cv::Point3d coordinate, cv::Scalar color)
{
    obj.push_back(std::make_pair<cv::Point3d, cv::Scalar>(coordinate, color));
}

static void init_variables(void)
{
    int i;

    insert_color_point(camera_aixs, cv::Point3d(1.0, 0.0,  0.0), cv::Scalar(0,     0, 255));
    insert_color_point(camera_aixs, cv::Point3d(0.0, 1.0,  0.0), cv::Scalar(0,   255, 255));
    insert_color_point(camera_aixs, cv::Point3d(0.0, 0.0, -1.0), cv::Scalar(255,   0, 255));
    camera_pos = cv::Point3d(0.0, 0.0, 300.0);

    for (i = 0; i < 3; i++)
        insert_color_point(world_aixs, world_aixs_data[i].coordinate, world_aixs_data[i].color);

    for (i = 0; i < 4; i++)
        insert_color_point(g_obj, g_obj_data[i].coordinate, g_obj_data[i].color);
}

static cv::Point3d rotating_on_x_aixs(cv::Point3d &point, double angle, double sin_angle, double cos_angle)
{
    cv::Point3d new_point;

    if (angle != 0.0) {
        sin_angle = sin(angle);
        cos_angle = cos(angle);
    } else if (sin_angle != 0.0) {
        cos_angle = sqrt(1 - (sin_angle * sin_angle));
    } else if (cos_angle != 0.0) {
        sin_angle = sqrt(1 - (cos_angle * cos_angle));
    } else {
        return point;
    }

    new_point.x = point.x;
    new_point.y = point.y * cos_angle - point.z * sin_angle;
    new_point.z = point.y * sin_angle + point.z * cos_angle;

    return new_point;
}

static cv::Point3d rotating_on_y_aixs(cv::Point3d &point, double angle, double sin_angle, double cos_angle)
{
    cv::Point3d new_point;

    if (angle != 0.0) {
        sin_angle = sin(angle);
        cos_angle = cos(angle);
    } else if (sin_angle != 0.0) {
        cos_angle = sqrt(1 - (sin_angle * sin_angle));
    } else if (cos_angle != 0.0) {
        sin_angle = sqrt(1 - (cos_angle * cos_angle));
    } else {
        return point;
    }

    new_point.x = point.x * cos_angle + point.z * sin_angle;
    new_point.y = point.y;
    new_point.z = point.z * cos_angle - point.x * sin_angle;

    return new_point;
}

static cv::Point3d rotating_on_z_aixs(cv::Point3d &point, double angle, double sin_angle, double cos_angle)
{
    cv::Point3d new_point;

    if (angle != 0.0) {
        sin_angle = sin(angle);
        cos_angle = cos(angle);
    } else if (sin_angle != 0.0) {
        cos_angle = sqrt(1 - (sin_angle * sin_angle));
    } else if (cos_angle != 0.0) {
        sin_angle = sqrt(1 - (cos_angle * cos_angle));
    } else {
        return point;
    }

    new_point.x = point.x * cos_angle - point.y * sin_angle;
    new_point.y = point.x * sin_angle + point.y * cos_angle;
    new_point.z = point.z;

    return new_point;
}

//          Y
//          |
//          |
//          |
//        O |________X
//          /
//         /
//        /
//       Z
static cv::Point3d rotating_point(cv::Point3d &point, cv::Point3d aixs_head, cv::Point3d aixs_tail, double angle)
{
    cv::Point3d origined_aixs, origined_point, result_point(0.0, 0.0, 0.0);
    cv::Point3d first_rotated_aixs, first_rotated_point, second_rotated_point;
    //cv::Point3d second_rotated_aixs;
    double mod_xz, mod_yz;

    // move to origin
    origined_aixs = aixs_head - aixs_tail;
    origined_point = point - aixs_tail;

    if ((origined_aixs.y == 0.0) && (origined_aixs.z == 0.0)) {
        origined_point = rotating_on_x_aixs(origined_point, angle, 0.0, 0.0);
        result_point = origined_point + aixs_tail;
        return result_point;
    } else if ((origined_aixs.x == 0.0) && (origined_aixs.z == 0.0)) {
        origined_point = rotating_on_y_aixs(origined_point, angle, 0.0, 0.0);
        result_point = origined_point + aixs_tail;
        return result_point;
    } else if ((origined_aixs.x == 0.0) && (origined_aixs.y == 0.0)) {
        origined_point = rotating_on_z_aixs(origined_point, angle, 0.0, 0.0);
        result_point = origined_point + aixs_tail;
        return result_point;
    }

    // aixs rotating to XOZ plane
    mod_yz = sqrt(origined_aixs.y * origined_aixs.y + origined_aixs.z * origined_aixs.z);
    first_rotated_aixs = rotating_on_x_aixs(origined_aixs, 0.0, origined_aixs.y/mod_yz, 0.0);
    first_rotated_point = rotating_on_x_aixs(origined_point, 0.0, origined_aixs.y/mod_yz, 0.0);

    // aixs rotating to coincide with Z aixs
    mod_xz = sqrt(first_rotated_aixs.x * first_rotated_aixs.x + first_rotated_aixs.z * first_rotated_aixs.z);
    //second_rotated_aixs = rotating_on_y_aixs(first_rotated_aixs, 0.0, -1 * first_rotated_aixs.x/mod_xz, 0.0);
    second_rotated_point = rotating_on_y_aixs(first_rotated_point, 0.0, -1 * first_rotated_aixs.x/mod_xz, 0.0);

    // rotating on Z aixs
    second_rotated_point = rotating_on_z_aixs(second_rotated_point, angle, 0.0, 0.0);

    // inverse rotating on Y aixs
    first_rotated_point = rotating_on_y_aixs(second_rotated_point, 0.0, first_rotated_aixs.x/mod_xz, 0.0);

    // inverse rotating on X aixs
    origined_point = rotating_on_x_aixs(first_rotated_point, 0.0, -1 * origined_aixs.y/mod_yz, 0.0);

    // move back
    result_point = origined_point + aixs_tail;

    return result_point;
}

static void rotating_object(Color_Points &object, Color_Points &output, bool in_place, cv::Point3d aixs_head, cv::Point3d aixs_tail, double angle)
{
    Color_Points *p;
    unsigned int i;

    if (in_place) {
        p = &object;
    } else {
        output = object;
        p = &output;
    }

    if (angle == 0.0)
        return;

    for (i = 0; i < (*p).size(); i++)
        (*p)[i].first = rotating_point((*p)[i].first, aixs_head, aixs_tail, angle);
}

static void draw_aixs(cv::Mat &image, Color_Points &aixs)
{
    cv::Point origin(image.cols/2, image.rows/2), head;
    int lineType = cv::LINE_AA, thickness = 1, scale = 100;

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

    rotating_object(world_aixs, output, false, world_aixs[0].first, cv::Point3d(0.0, 0.0, 0.0), PI/8);
    rotating_object(output, output, false, output[1].first, cv::Point3d(0.0, 0.0, 0.0), -PI/8);
    draw_aixs(image, output);

    rotating_object(g_obj, output, true, cv::Point3d(1.0, 2.0, 0.0), cv::Point3d(0.0, 0.0, 0.0), PI/512);
    draw_obj(image, g_obj);

    s << "aixs_x=" << world_aixs[0].first;
    text = s.str();
    text_size = cv::getTextSize(text, text_font, text_scale, text_thickness, NULL);
    cv::putText(image, text, cv::Point(10, 20), text_font, text_scale, text_color, text_thickness, cv::LINE_AA, false);

    s.str("");
    s << "aixs_y=" << world_aixs[1].first;
    text = s.str();
    text_size = cv::getTextSize(text, text_font, text_scale, text_thickness, NULL);
    cv::putText(image, text, cv::Point(10, 20 + (10 + text_size.height)), text_font, text_scale, text_color, text_thickness, cv::LINE_AA, false);

    s.str("");
    s << "aixs_z=" << world_aixs[2].first;
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

void temp_test()
{
    cv::Point3d a(1.0, 1.0, 1.0), b(0.0, 0.0, 0.0), c(0.0, 0.0, 0.0), d(0.0, 0.0, 0.0), e(0.0, 0.0, 0.0);

    b = rotating_point(a, cv::Point3d(1.0, 0.001, 0.0), cv::Point3d(0.0, 0.0, 0.0), PI/4);
    c = rotating_on_x_aixs(a, PI/4, 0.0, 0.0);
    d = rotating_on_y_aixs(a, PI/4, 0.0, 0.0);
    e = rotating_on_z_aixs(a, PI/4, 0.0, 0.0);

    std::cout << "r:" << b << std::endl;
    std::cout << "x:" << c << std::endl;
    std::cout << "y:" << d << std::endl;
    std::cout << "z:" << e << std::endl;
}

int main(int argc, char **argv)
{
    struct stream_source stream_src;
    pthread_t t;
    int ret = -1;

    cv::Mat image = cv::Mat::eye(SCREEN_H, SCREEN_W, CV_8UC3);

    //temp_test();
    //return 0;

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
