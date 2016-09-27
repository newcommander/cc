#ifndef RTP_H
#define RTP_H

struct rtp_pkt {
    uint32_t header;
    uint32_t timestamp;
    uint32_t ssrc;
    uint8_t payload[1400];
};

#include "session.h"

int init_rtp_handle(uv_udp_t *handle);
void del_session_from_rtp_list(struct session *se);
int add_session_to_rtp_list(struct session *se);
void rtp_handle_close_cb(uv_handle_t *handle);
void rtp_stop();

#endif /* RTP_H */
