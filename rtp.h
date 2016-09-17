#ifndef RTP_H
#define RTP_H

struct rtp_pkt {
    uint32_t header;
    uint32_t timestamp;
    uint32_t ssrc;
    uint8_t payload[1400];
};

#endif /* RTP_H */
