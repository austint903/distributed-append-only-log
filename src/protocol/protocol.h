#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

static constexpr uint16_t SERVER_PORT = 9999;


struct ProducerPacket {
    uint32_t seq;
    uint32_t payload_len;
} __attribute__((packed));

struct AckPacket {
    uint32_t producer_seq;
    uint64_t committed_seq;
} __attribute__((packed));

#endif // PROTOCOL_H
