#ifndef RTCP_H
#define RTCP_H

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

extern void rtcp_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
extern void rtcp_recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags);
extern void del_from_rtcp_list(struct list_head *list);
extern void add_to_rtcp_list(struct list_head *list);
extern void add_src_desc(struct sr_rtcp_pkt *pkt, struct session *se);
extern void set_pkt_header(struct sr_rtcp_pkt *pkt);

#endif /* RTCP_H */
