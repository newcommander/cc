#include "session.h"

struct sr_rtcp_pkt {
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

int add_session_to_rtcp_list(struct session *se);
