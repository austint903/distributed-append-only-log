#ifndef PRODUCER_H
#define PRODUCER_H

#include <cstdint>
#include <string>

static constexpr const char* SERVER_IP   = "127.0.0.1";
static constexpr uint16_t    SERVER_PORT = 9999;
static constexpr int         TIMEOUT_SEC = 2;

struct ProducerPacket {
    uint32_t seq;
    uint32_t payload_len;
} __attribute__((packed));

struct AckPacket {
    uint32_t producer_seq;
    uint64_t committed_seq;
} __attribute__((packed));

std::string random_payload(size_t len);

#endif // PRODUCER_H
