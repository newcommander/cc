#ifndef ENCODER_H
#define ENCODER_H

#include "rtsp.h"

int encoder_init(rtsp_session *rs);
void encoder_deinit(rtsp_session *rs);
int video_encoding(rtsp_session *rs, unsigned char *data, int *len);

#endif
