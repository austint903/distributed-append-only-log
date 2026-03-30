#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

#include "../src/protocol/protocol.h"

#ifndef __APPLE__
#include <endian.h>
#endif

TEST(ProducerPacket, SizeIs8Bytes) {
    EXPECT_EQ(sizeof(ProducerPacket), 8u);
}

TEST(AckPacket, SizeIs12Bytes) {
    EXPECT_EQ(sizeof(AckPacket), 12u);
}

TEST(ProducerPacket, SeqFieldIsAtOffset0) {
    EXPECT_EQ(offsetof(ProducerPacket, seq), 0u);
}

TEST(ProducerPacket, PayloadLenFieldIsAtOffset4) {
    EXPECT_EQ(offsetof(ProducerPacket, payload_len), 4u);
}

TEST(AckPacket, ProducerSeqFieldIsAtOffset0) {
    EXPECT_EQ(offsetof(AckPacket, producer_seq), 0u);
}

TEST(AckPacket, CommittedSeqFieldIsAtOffset4) {
    EXPECT_EQ(offsetof(AckPacket, committed_seq), 4u);
}

TEST(ProducerPacket, BigEndianBytesDeserialiseToHostValues) {
    uint8_t raw[8];
    uint32_t seq_be = htonl(42);
    uint32_t len_be = htonl(100);
    memcpy(raw + 0, &seq_be, 4);
    memcpy(raw + 4, &len_be, 4);

    ProducerPacket pkt{};
    memcpy(&pkt, raw, sizeof(pkt));

    EXPECT_EQ(ntohl(pkt.seq),         42u);
    EXPECT_EQ(ntohl(pkt.payload_len), 100u);
}

TEST(AckPacket, HostValuesSerialiseToReadableRawBytes) {
    AckPacket ack{};
    ack.producer_seq  = htonl(7u);
    ack.committed_seq = htobe64(9876543210ULL);

    uint8_t raw[12];
    memcpy(raw, &ack, sizeof(ack));

    uint32_t raw_seq;
    uint64_t raw_committed;
    memcpy(&raw_seq,       raw + 0, 4);
    memcpy(&raw_committed, raw + 4, 8);

    EXPECT_EQ(ntohl(raw_seq),         7u);
    EXPECT_EQ(be64toh(raw_committed), 9876543210ULL);
}

TEST(ProducerPacket, FrameSizeArithmeticIsCorrect) {
    constexpr uint32_t payload_len = 32;
    EXPECT_EQ(sizeof(ProducerPacket) + payload_len, 40u);
}
