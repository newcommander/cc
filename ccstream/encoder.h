#ifndef ENCODER_H
#define ENCODER_H

#include "session.h"
#include "uri.h"

int sample_frame(struct session *se, unsigned char *out_buf, int *out_buf_len);
void register_encoders();
int encoder_init(struct session *se);
void encoder_deinit(struct session *se);
int get_media_config(struct Uri *uri, char *encoder_name, char *buf, int size);

#endif /* ENCODER_H */
