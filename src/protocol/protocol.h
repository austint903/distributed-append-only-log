#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

#ifdef __APPLE__
#include <machine/endian.h>
#include <libkern/OSByteOrder.h>
#define be64toh(x) OSSwapBigToHostInt64(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#endif

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
