#include "session.h"

struct rtp_pkt {
    uint32_t header;
    uint32_t timestamp;
    uint32_t ssrc;
    uint8_t payload[1400];
};

int add_session_to_rtp_list(struct session *se);