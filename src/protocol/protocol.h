#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

static constexpr uint16_t SERVER_PORT = 9999;

// Producer → Server: fixed header followed by `payload_len` bytes of payload.
struct ProducerPacket {
    uint32_t seq;
    uint32_t payload_len;
} __attribute__((packed));

// Server → Producer: acknowledgement after the record is committed to the log.
struct AckPacket {
    uint32_t producer_seq;   // echoes ProducerPacket::seq
    uint64_t committed_seq;  // log sequence number assigned to this record
} __attribute__((packed));

#endif // PROTOCOL_H
