#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

#include "../src/util/util.h"


TEST(Crc32Compute, KnownStringProducesStandardChecksum) {
    const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    EXPECT_EQ(crc32_compute(data, sizeof(data)), 0xCBF43926u);
}

TEST(Crc32Compute, EmptyInputReturnsZero) {
    const uint8_t placeholder = 0;
    EXPECT_EQ(crc32_compute(&placeholder, 0), 0x00000000u);
}

TEST(Crc32Compute, SameInputProducesSameOutputOnRepeatedCalls) {
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_EQ(crc32_compute(data, sizeof(data)),
              crc32_compute(data, sizeof(data)));
}

TEST(Crc32Compute, DifferentInputsProduceDifferentChecksums) {
    const uint8_t a[] = {0xAA, 0xBB};
    const uint8_t b[] = {0xCC, 0xDD};
    EXPECT_NE(crc32_compute(a, sizeof(a)), crc32_compute(b, sizeof(b)));
}

TEST(Crc32Compute, SingleBitFlipChangesChecksum) {
    const uint8_t original[] = {0b10101010};
    const uint8_t flipped[]  = {0b10101011};
    EXPECT_NE(crc32_compute(original, 1), crc32_compute(flipped, 1));
}

TEST(Crc32Compute, ChecksumCoversBytesAfterTheFirst) {
    const uint8_t a[] = {0x00, 0xFF};
    const uint8_t b[] = {0x00, 0x00};
    EXPECT_NE(crc32_compute(a, sizeof(a)), crc32_compute(b, sizeof(b)));
}

TEST(Crc32Compute, LengthIsRespected) {
    const uint8_t data[] = {0x42, 0x42};
    EXPECT_NE(crc32_compute(data, 1), crc32_compute(data, 2));
}
