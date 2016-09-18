#ifndef RTP_H
#define RTP_H

struct rtp_pkt {
    uint32_t header;
    uint32_t timestamp;
    uint32_t ssrc;
    uint8_t payload[1400];
};

extern int init_rtp_handle(uv_udp_t *handle);
extern void del_from_rtp_list(struct session *se);

#endif /* RTP_H */
