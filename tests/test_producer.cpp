#include <gtest/gtest.h>
#include <cctype>
#include <string>

#include "../tools/producer/producer.h"

TEST(RandomPayload, ZeroLengthReturnsEmptyString) {
    EXPECT_TRUE(random_payload(0).empty());
}

TEST(RandomPayload, RequestedLengthIsExact) {
    EXPECT_EQ(random_payload(1).size(),   1u);
    EXPECT_EQ(random_payload(32).size(),  32u);
    EXPECT_EQ(random_payload(256).size(), 256u);
}

TEST(RandomPayload, LargeRequestedLengthIsExact) {
    EXPECT_EQ(random_payload(10000).size(), 10000u);
}

TEST(RandomPayload, AllCharactersAreAlphanumeric) {
    const std::string s = random_payload(512);
    for (unsigned char c : s) {
        EXPECT_TRUE(std::isalnum(c))
            << "Non-alphanumeric byte: 0x" << std::hex << static_cast<int>(c);
    }
}
