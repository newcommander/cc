#define __STDC_CONSTANT_MACROS

#include <string>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

extern "C" {
#include <stdint.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include "../ccstream.h"
}

#define OPENCV_W 640
#define OPENCV_H 600

int opencv_draw(void *_frame, int screen_h, int screen_w, void *arg)
{
    AVFrame *frame = (AVFrame*)_frame;
    struct stream_source *stream_src = (struct stream_source*)arg;
	cv::Mat *image = NULL;
    int object_w, object_h;
    //int channle, num;
    int width, height, y, x;

	if (!frame || !stream_src || !stream_src->data) {
        printf("%s: Invalid parameter\n", __func__);
		return -1;
	}

	image = (cv::Mat*)stream_src->data;
    object_w = stream_src->dim[0];
    object_h = stream_src->dim[1];
    //channle = stream_src->dim[2];
    //num = stream_src->dim[3];

    width  = screen_w > object_w ? object_w : screen_w;
    height = screen_h > object_h ? object_h : screen_h;
	printf("w=%d, h=%d\n", object_w, object_h);

    pthread_rwlock_rdlock(&stream_src->sample_lock);
    for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			frame->data[y * frame->linesize[0] + x] = (uint8_t*)image->data + y * object_h * 3 + x * 3;
//        memcpy(frame->data[0] + y * frame->linesize[0], (uint8_t*)stream_src->data + y * object_w, width);
    pthread_rwlock_unlock(&stream_src->sample_lock);

	return 0;
}

struct uri_entry entrys[] = {
    { "opencv", "trackID=1", OPENCV_W, OPENCV_H, 30, NULL, opencv_draw },
    { NULL, NULL, 0, 0, 0, NULL, NULL }
};

void MyEllipse( cv::Mat img, double angle )
{
  int thickness = 2;
  int lineType = 8;

  cv::ellipse( img,
       cv::Point( OPENCV_W/2, OPENCV_H/2 ),
       cv::Size( OPENCV_W/4, OPENCV_H/16 ),
       angle,
       0,
       360,
       cv::Scalar( 255, 0, 0 ),
       thickness,
       lineType );
}

int main(int argc, char **argv)
{
    struct stream_source stream_src;
    pthread_t t;

	cv::Mat image = cv::Mat::eye(OPENCV_W, OPENCV_H, CV_8UC3);
    printf("rows=%d, cols=%d\n", image.rows, image.cols);
    printf("dims=%d, step1=%lu, step2=%lu\n", image.dims, image.step[0], image.step[1]);
    MyEllipse(image, 0);

    stream_src.data = &image;
    stream_src.dim[0] = OPENCV_W;
    stream_src.dim[1] = OPENCV_H;
    stream_src.dim[2] = 1;
    stream_src.dim[3] = 1;
    pthread_rwlock_init(&stream_src.sample_lock, NULL);
    entrys[0].data = &stream_src;

    if (pthread_create(&t, NULL, cc_stream, entrys) != 0) {
        printf("%s: Creating cc_stream thread failed", __func__);
        return -1;
    }

    pthread_join(t, NULL);
    pthread_rwlock_destroy(&stream_src.sample_lock);

    return 0;
}
