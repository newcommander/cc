#ifndef RTCP_H
#define RTCP_H

struct rtcp_sr_pkt {
    uint32_t header;
    uint32_t ssrc;
    uint32_t ntp_timestamp_msw;
    uint32_t ntp_timestamp_lsw;
    uint32_t rtp_timestamp;
    uint32_t packet_count;
    uint32_t octet_count;
    // with no report blocks
    uint32_t extension[16]; // TODO: fixed size ?
};

#include "session.h"

int init_rtcp_handle(uv_udp_t *handle);
void del_session_from_rtcp_list(struct session *se);
int add_session_to_rtcp_list(struct session *se);
void rtcp_handle_close_cb(uv_handle_t *handle);

#endif /* RTCP_H */
