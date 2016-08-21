#ifndef ENCODER_H
#define ENCODER_H

extern void register_encoders();
extern int encoder_init(struct session *se);
extern void encoder_deinit(struct session *se);
extern int sample_frame(struct session *se, unsigned char *data, int *len);

#endif
