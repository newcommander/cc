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
#include "ccstream/ccstream.h"
#include "ccstream/common.h"
}

#define SCREEN_W 600
#define SCREEN_H 400

#define PI 3.14159
#define MOST_DEEP -1.0e100

#define MAX_VERTEX_OF_PLANE 1024

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

static struct Aixs {
    cv::Point3d origin;
    cv::Point3d x_aixs;
    cv::Point3d y_aixs;
    cv::Point3d z_aixs;
} world_aixs = {
    cv::Point3d(0.0, 0.0, 0.0),
    cv::Point3d(1.0, 0.0, 0.0),
    cv::Point3d(0.0, 1.0, 0.0),
    cv::Point3d(0.0, 0.0, 1.0)
}, camera_aixs = {
    cv::Point3d(0.0, 0.0, 30.0),
    cv::Point3d(1.0, 0.0, 30.0),
    cv::Point3d(0.0, 1.0, 30.0),
    cv::Point3d(0.0, 0.0, 31.0)
};

typedef std::vector<std::pair<std::vector<cv::Point3d>, cv::Scalar> > Color_Planes;
struct Object {
    std::string name;
    Color_Planes planes;
    struct Aixs master_aixs;
    std::vector<std::pair<std::string, struct Aixs> > slaver_aixs;
};
static std::vector<Color_Planes> g_objects;

static cv::Point3d camera_angle_on_world_aixs = cv::Point3d(0.0, 0.0, 0.0);
static cv::Point3d camera_angle_self = cv::Point3d(0.0, 0.0, 0.0);

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
static void rotating_point(cv::Point3d &point, cv::Point3d aixs_head, cv::Point3d aixs_tail, double angle)
{
    cv::Point3d origined_aixs, origined_point;
    cv::Point3d first_rotated_aixs, first_rotated_point, second_rotated_point;
    //cv::Point3d second_rotated_aixs;
    double mod_xz, mod_yz;

    // move to origin
    origined_aixs = aixs_head - aixs_tail;
    origined_point = point - aixs_tail;

    if ((origined_aixs.y == 0.0) && (origined_aixs.z == 0.0)) {
        origined_point = rotating_on_x_aixs(origined_point, angle, 0.0, 0.0);
        point = origined_point + aixs_tail;
        return;
    } else if ((origined_aixs.x == 0.0) && (origined_aixs.z == 0.0)) {
        origined_point = rotating_on_y_aixs(origined_point, angle, 0.0, 0.0);
        point = origined_point + aixs_tail;
        return;
    } else if ((origined_aixs.x == 0.0) && (origined_aixs.y == 0.0)) {
        origined_point = rotating_on_z_aixs(origined_point, angle, 0.0, 0.0);
        point = origined_point + aixs_tail;
        return;
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
    point = origined_point + aixs_tail;
}

static void rotating_object(Color_Planes &object, cv::Point3d aixs_head, cv::Point3d aixs_tail, double angle)
{
    size_t i, j;

    if (angle == 0.0)
        return;

    for (i = 0; i < object.size(); i++) // each plane of object
        for (j = 0; j < object[i].first.size(); j++) // each vertex of plane
            rotating_point(object[i].first[j], aixs_head, aixs_tail, angle);
}

static void rotating_to_camera_view(Color_Planes &object)
{
    rotating_object(object, world_aixs.x_aixs, world_aixs.origin, camera_angle_on_world_aixs.x);
    rotating_object(object, world_aixs.y_aixs, world_aixs.origin, camera_angle_on_world_aixs.y);
    rotating_object(object, world_aixs.z_aixs, world_aixs.origin, camera_angle_on_world_aixs.z);

    rotating_object(object, camera_aixs.x_aixs, camera_aixs.origin, camera_angle_self.x);
    rotating_object(object, camera_aixs.y_aixs, camera_aixs.origin, camera_angle_self.y);
    rotating_object(object, camera_aixs.z_aixs, camera_aixs.origin, camera_angle_self.z);
}

static void draw_info(cv::Mat &image)
{
    std::stringstream s;
    cv::String text;
    cv::Size text_size;
    cv::Scalar text_color = cv::Scalar(173, 121, 54);
    int text_font = cv::FONT_HERSHEY_DUPLEX;
    double text_scale = 0.5;
    int text_thickness = 1;

    s << "camera_origin: " << camera_aixs.origin;
    text = s.str();
    text_size = cv::getTextSize(text, text_font, text_scale, text_thickness, NULL);
    cv::putText(image, text, cv::Point(10, 20), text_font, text_scale, text_color, text_thickness, cv::LINE_AA, false);

    s.str("");
    s << "camera_angle: " << camera_angle_self << " / " << camera_angle_on_world_aixs;
    text = s.str();
    cv::putText(image, text, cv::Point(10, 20+10+text_size.height), text_font, text_scale, text_color, text_thickness, cv::LINE_AA, false);
}

static void draw_aixs(cv::Mat &image, struct Aixs &aixs, cv::Point2d offset)
{
    cv::Point2d origin, head;
    struct Aixs new_aixs;
    int lineType = cv::LINE_AA, thickness = 1, scale = 30;

    new_aixs.origin = aixs.origin;
    new_aixs.x_aixs = aixs.x_aixs * scale;
    new_aixs.y_aixs = aixs.y_aixs * scale;
    new_aixs.z_aixs = aixs.z_aixs * scale;

    if (camera_angle_on_world_aixs.x != 0.0) {
        rotating_point(new_aixs.x_aixs, world_aixs.x_aixs, world_aixs.origin, camera_angle_on_world_aixs.x);
        rotating_point(new_aixs.y_aixs, world_aixs.x_aixs, world_aixs.origin, camera_angle_on_world_aixs.x);
        rotating_point(new_aixs.z_aixs, world_aixs.x_aixs, world_aixs.origin, camera_angle_on_world_aixs.x);
    }
    if (camera_angle_on_world_aixs.y != 0.0) {
        rotating_point(new_aixs.x_aixs, world_aixs.y_aixs, world_aixs.origin, camera_angle_on_world_aixs.y);
        rotating_point(new_aixs.y_aixs, world_aixs.y_aixs, world_aixs.origin, camera_angle_on_world_aixs.y);
        rotating_point(new_aixs.z_aixs, world_aixs.y_aixs, world_aixs.origin, camera_angle_on_world_aixs.y);
    }
    if (camera_angle_on_world_aixs.z != 0.0) {
        rotating_point(new_aixs.x_aixs, world_aixs.z_aixs, world_aixs.origin, camera_angle_on_world_aixs.z);
        rotating_point(new_aixs.y_aixs, world_aixs.z_aixs, world_aixs.origin, camera_angle_on_world_aixs.z);
        rotating_point(new_aixs.z_aixs, world_aixs.z_aixs, world_aixs.origin, camera_angle_on_world_aixs.z);
    }

    offset.y *= -1;
    origin = cv::Point2d(new_aixs.origin.x, new_aixs.origin.y) + cv::Point2d(image.cols/2, image.rows/2) + offset;
    head = cv::Point2d(new_aixs.x_aixs.x + origin.x, origin.y - new_aixs.x_aixs.y);
    cv::arrowedLine(image, origin, head, cv::Scalar(0, 0, 255), thickness, lineType);
    head = cv::Point2d(new_aixs.y_aixs.x + origin.x, origin.y - new_aixs.y_aixs.y);
    cv::arrowedLine(image, origin, head, cv::Scalar(0, 255, 0), thickness, lineType);
    head = cv::Point2d(new_aixs.z_aixs.x + origin.x, origin.y - new_aixs.z_aixs.y);
    cv::arrowedLine(image, origin, head, cv::Scalar(255, 0, 0), thickness, lineType);
}

static void make_plane_deep_info(cv::Mat deep_info_plane, std::vector<cv::Point3d> &points, cv::Point2d offset)
{
    cv::Point3d e, f;
    double a, b, c, d; // plane: ax + by + cz + d = 0;
    int rows = deep_info_plane.rows;
    int cols = deep_info_plane.cols;
    int i, j;
    double *p;

    // a plane in 3D space can be ascertained by at lest 3 points
    if (points.size() < 3)
        return;

    e = cv::Point3d(points[1].x - points[0].x, points[1].y - points[0].y, points[1].z - points[0].z);
    f = cv::Point3d(points[2].x - points[0].x, points[2].y - points[0].y, points[2].z - points[0].z);

    a = e.y * f.z - e.z * f.y;
    b = e.z * f.x - e.x * f.z;
    c = e.x * f.y - e.y * f.x;
    d = points[0].x * (e.z * f.y - e.y * f.z) +
        points[0].y * (e.x * f.z - e.z * f.x) +
        points[0].z * (e.y * f.x - e.x * f.y);

    for (j = 0; j < rows; j++) {
        p = deep_info_plane.ptr<double>(j);
        for (i = 0; i < cols; i++) {
            p[i] = -1 * (a * (offset.x + 0.5 + i) + b * (offset.y + 0.5 - j) + d) / c;
        }
    }
}

static void draw_plane_mask(cv::Mat &plane_mask, std::vector<cv::Point3d> &points, cv::Point2d offset)
{
    cv::Point pts[1][MAX_VERTEX_OF_PLANE];
    const cv::Point *ppt[1] = { pts[0] };
    int num[1], i;

    if ((points.size() < 3) || (points.size() > MAX_VERTEX_OF_PLANE)) {
        printf("%s(): Invalid plane, point_num=%lu\n", __func__, points.size());
        return;
    }
    if (plane_mask.channels() != 1) {
        printf("%s(): Invalid mask Mat, channels_num=%d\n", __func__, plane_mask.channels());
        return;
    }

    num[0] = points.size();
    for (i = 0; i < num[0]; i++)
        pts[0][i] = cv::Point(points[i].x - offset.x, offset.y - points[i].y);

    cv::fillPoly(plane_mask, ppt, num, 1, cv::Scalar(1), cv::LINE_AA);
}

static void draw_plane_image(cv::Mat &plane_image, std::vector<cv::Point3d> &points, cv::Point2d offset, cv::Scalar &color)
{
    cv::Point pts[1][MAX_VERTEX_OF_PLANE];
    const cv::Point *ppt[1] = { pts[0] };
    int num[1], i;

    if ((points.size() < 3) || (points.size() > MAX_VERTEX_OF_PLANE)) {
        printf("%s(): Invalid plane, point_num=%lu\n", __func__, points.size());
        return;
    }
    if (plane_image.channels() != 3) {
        printf("%s(): Invalid mask Mat, channels_num=%d\n", __func__, plane_image.channels());
        return;
    }

    num[0] = points.size();
    for (i = 0; i < num[0]; i++)
        pts[0][i] = cv::Point(points[i].x - offset.x, offset.y - points[i].y);

    cv::fillPoly(plane_image, ppt, num, 1, color, cv::LINE_AA);
}

static void draw_plane(cv::Mat &image, cv::Mat &deep_info, std::vector<cv::Point3d> &points, cv::Scalar &color)
{
    cv::Mat plane_area, plane_image;
    cv::Mat deep_info_area, deep_info_plane;
    cv::Mat plane_mask, plane_mask_tmp, plane_mask_3c, plane_mask_double;
    cv::Mat plane_mask_array[3];
    cv::Point3d origin(image.cols/2, image.rows/2, 0.0);
    double min_x, min_y, max_x, max_y;
    size_t i, points_count = points.size();

    min_x = max_x = points[0].x;
    min_y = max_y = points[0].y;

    for (i = 0; i < points_count; i++) {
        if (points[i].x < min_x)
            min_x = points[i].x;
        if (points[i].x > max_x)
            max_x = points[i].x;
        if (points[i].y < min_y)
            min_y = points[i].y;
        if (points[i].y > max_y)
            max_y = points[i].y;
    }
    min_x = floor(min_x);
    max_x = ceil(max_x);
    min_y = floor(min_y);
    max_y = ceil(max_y);

    if ((min_x >= max_x) || (min_y >= max_y))
        return;

    plane_area = image(cv::Range(origin.y - max_y, origin.y - min_y), cv::Range(origin.x + min_x, origin.x + max_x));
    deep_info_area = deep_info(cv::Range(origin.y - max_y, origin.y - min_y), cv::Range(origin.x + min_x, origin.x + max_x));

    deep_info_plane = cv::Mat(max_y - min_y, max_x - min_x, CV_64FC1, cv::Scalar(MOST_DEEP));
    make_plane_deep_info(deep_info_plane, points, cv::Point2d(min_x, max_y));

    plane_mask     = cv::Mat(max_y - min_y, max_x - min_x, CV_8UC1, cv::Scalar(0));
    plane_mask_tmp = cv::Mat(max_y - min_y, max_x - min_x, CV_8UC1, cv::Scalar(0));

    plane_mask = (deep_info_area < deep_info_plane) / 255;
    draw_plane_mask(plane_mask_tmp, points, cv::Point2d(min_x, max_y));
    plane_mask = plane_mask.mul(plane_mask_tmp);
    plane_mask.convertTo(plane_mask_double, CV_64FC1);
    plane_mask_array[0] = plane_mask;
    plane_mask_array[1] = plane_mask;
    plane_mask_array[2] = plane_mask;
    cv::merge(plane_mask_array, 3, plane_mask_3c);

    plane_image = cv::Mat(max_y - min_y, max_x - min_x, CV_8UC3, cv::Scalar(0, 0, 0));
    draw_plane_image(plane_image, points, cv::Point2d(min_x, max_y), color);

    plane_area = plane_area.mul((~plane_mask_3c) - cv::Scalar(254, 254, 254)) + plane_image.mul(plane_mask_3c);
    deep_info_area = deep_info_area.mul((plane_mask_double - 1.0), -1.0) + deep_info_plane.mul(plane_mask_double);
}

static void draw_object(cv::Mat &image, cv::Mat &deep_info, Color_Planes &object)
{
    Color_Planes new_object = object;
    size_t i;

    rotating_to_camera_view(new_object);

    for (i = 0; i < new_object.size(); i ++)
        draw_plane(image, deep_info, new_object[i].first, new_object[i].second);
}

static void opencv_draw(cv::Mat &image)
{
    cv::Mat deep_info(image.size(), CV_64FC1, cv::Scalar(MOST_DEEP));
    cv::rectangle(image, cv::Point(0, 0), cv::Point(image.cols, image.rows), cv::Scalar(86, 60, 27), -1);

    draw_object(image, deep_info, g_objects[0]);

    draw_aixs(image, world_aixs, cv::Point2d(-250, -150));

    draw_info(image);

    camera_angle_on_world_aixs.x += 0.1;
    camera_angle_on_world_aixs.y += 0.2;
    camera_angle_on_world_aixs.z += 0.1;
}

static void make_point3d(std::string line, std::vector<cv::Point3d> &vertexes)
{
    double x, y, z, scale = 50;
    // XXX precision loss
    sscanf(line.c_str(), "%lf %lf %lf", &x, &y, &z);
    vertexes.push_back(cv::Point3d(x * scale, y * scale, z * scale));
}

static void make_plane(std::string line, cv::Scalar color, std::vector<cv::Point3d> &vertexes, Color_Planes &obj)
{
    std::vector<cv::Point3d> verts;
    int slash = 0, space = 0, count = 0, index;

    verts.clear();
    while (1) {
        slash = line.find('/', space);
        if (slash < 0)
            break;
        index = strtod(line.substr(space, slash - space).c_str(), NULL);
        verts.push_back(vertexes[index - 1]);
        count++;
        space = line.find(' ', slash);
    }
    if (count > MAX_VERTEX_OF_PLANE) {
        printf("%s: too many vertex of a plane, %d > max=%d, ignore this plane!\n", __func__, count, MAX_VERTEX_OF_PLANE);
        return;
    }
    obj.push_back(std::make_pair<std::vector<cv::Point3d>, cv::Scalar>(verts, color));
}

static int load_data(std::string file_name)
{
    std::ifstream file(file_name.c_str());
    std::string line, type;
    std::vector<cv::Point3d> vertexes;
    std::pair<std::vector<cv::Point3d>, cv::Scalar> plane;
    Color_Planes obj;
    cv::RNG rng(rand());

    if (file) {
        while (std::getline(file, line)) {
            if (line[0] == '#')
                continue;
            type = line.substr(0, line.find(' ', 0));
            if (type == "o") {
                obj.clear();
                vertexes.clear();
                while (std::getline(file, line)) {
                    if (line[0] == '#')
                        continue;
                    type = line.substr(0, line.find(' ', 0));
                    if (type == "v")
                        make_point3d(line.substr(line.find(' ', 0) + 1, line.length() - line.find(' ', 0)), vertexes);
                    else
                        break;
                }
                if (vertexes.size() == 0)
                    continue;
                while (std::getline(file, line)) {
                    if (line[0] == '#')
                        continue;
                    type = line.substr(0, line.find(' ', 0));
                    if (type == "f")
                        make_plane(line.substr(line.find(' ', 0) + 1, line.length() - line.find(' ', 0)),
                                cv::Scalar(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256)), vertexes, obj);
                    else if (type == "o")
                        break;
                }
                g_objects.push_back(obj);
            }
        }
    } else {
        std::cout << "No such file: " << file_name << std::endl;
        return -1;
    }

    return 0;
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

    load_data("data/default.obj");
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
