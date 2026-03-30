#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unistd.h>

#include "../src/server/append_log/append_log.h"
#include "../src/server/log_reader/log_reader.h"

class LogReaderTest : public ::testing::Test {
protected:
    std::string path_;
    AppendLog*  log_    = nullptr;
    LogReader*  reader_ = nullptr;

    void SetUp() override {
        path_ = std::string("/tmp/log_reader_") +
                ::testing::UnitTest::GetInstance()->current_test_info()->name() +
                ".bin";
        ::unlink(path_.c_str());
        log_    = new AppendLog(path_.c_str());
        reader_ = new LogReader(*log_, path_.c_str());
    }

    void TearDown() override {
        delete reader_;  reader_ = nullptr;
        delete log_;     log_    = nullptr;
        ::unlink(path_.c_str());
    }

    static std::vector<uint8_t> to_bytes(const std::string& s) {
        return {s.begin(), s.end()};
    }
};

TEST_F(LogReaderTest, ReadNonExistentSequenceReturnsNullopt) {
    auto result = reader_->read(0);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LogReaderTest, ReadSequenceAfterLastWrittenReturnsNullopt) {
    log_->append(to_bytes("only"));
    auto result = reader_->read(1);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LogReaderTest, ReadSequenceZeroReturnsCorrectPayload) {
    const auto payload = to_bytes("hello world");
    log_->append(payload);

    auto result = reader_->read(0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

TEST_F(LogReaderTest, MultipleDistinctSequencesReturnIndependentPayloads) {
    const auto p0 = to_bytes("alpha");
    const auto p1 = to_bytes("beta");
    const auto p2 = to_bytes("gamma");
    log_->append(p0);
    log_->append(p1);
    log_->append(p2);

    EXPECT_EQ(*reader_->read(0), p0);
    EXPECT_EQ(*reader_->read(1), p1);
    EXPECT_EQ(*reader_->read(2), p2);
}

TEST_F(LogReaderTest, BinaryPayloadIsPreservedExactly) {
    const std::vector<uint8_t> payload = {0x00, 0x01, 0x7F, 0xFE, 0xFF};
    log_->append(payload);

    auto result = reader_->read(0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

TEST_F(LogReaderTest, SequencesCanBeReadInReverseOrder) {
    const auto p0 = to_bytes("first");
    const auto p1 = to_bytes("second");
    log_->append(p0);
    log_->append(p1);

    EXPECT_EQ(*reader_->read(1), p1);
    EXPECT_EQ(*reader_->read(0), p0);
}

TEST_F(LogReaderTest, EmptyPayloadRoundTrips) {
    const std::vector<uint8_t> empty;
    log_->append(empty);

    auto result = reader_->read(0);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST_F(LogReaderTest, LargePayloadRoundTrips) {
    const std::vector<uint8_t> big(4096, 0xAB);
    log_->append(big);

    auto result = reader_->read(0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, big);
}
