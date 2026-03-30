#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#include "../src/util/util.h"

class ScanLogTest : public ::testing::Test {
protected:
    std::string path_;
    int fd_ = -1;

    void SetUp() override {
        path_ = std::string("/tmp/scan_log_") +
                ::testing::UnitTest::GetInstance()->current_test_info()->name() +
                ".bin";
        ::unlink(path_.c_str());
        fd_ = ::open(path_.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        ASSERT_GE(fd_, 0);
    }

    void TearDown() override {
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
        ::unlink(path_.c_str());
    }

    void write_valid_record(uint64_t seq, const std::vector<uint8_t>& payload) {
        RecordHeader hdr{};
        hdr.sequence_number = seq;
        hdr.payload_length  = static_cast<uint32_t>(payload.size());
        hdr.crc32           = crc32_compute(payload.data(), payload.size());
        ASSERT_EQ(::write(fd_, &hdr, sizeof(hdr)), (ssize_t)sizeof(hdr));
        if (!payload.empty())
            ASSERT_EQ(::write(fd_, payload.data(), payload.size()), (ssize_t)payload.size());
    }

    void write_raw(const void* data, size_t len) {
        ASSERT_EQ(::write(fd_, data, len), (ssize_t)len);
    }
};

TEST_F(ScanLogTest, EmptyFileReturnsZeroSeqAndZeroEnd) {
    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.next_seq,      0u);
    EXPECT_EQ(result.last_good_end, 0);
}

TEST_F(ScanLogTest, SingleValidRecordAdvancesNextSeqToOne) {
    write_valid_record(0, {'h', 'i'});

    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.next_seq, 1u);
}

TEST_F(ScanLogTest, SingleValidRecordSetsLastGoodEndToRecordSize) {
    const std::vector<uint8_t> payload = {'h', 'i'};
    write_valid_record(0, payload);

    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.last_good_end,
              static_cast<off_t>(sizeof(RecordHeader) + payload.size()));
}

TEST_F(ScanLogTest, MultipleValidRecordsReturnsLastSeqPlusOne) {
    write_valid_record(0, {0x01});
    write_valid_record(1, {0x02, 0x03});
    write_valid_record(2, {0x04, 0x05, 0x06});

    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.next_seq, 3u);
}

TEST_F(ScanLogTest, MultipleValidRecordsEndsAtCumulativeFileSize) {
    const std::vector<uint8_t> p0 = {0xAA};
    const std::vector<uint8_t> p1 = {0xBB, 0xCC};
    write_valid_record(0, p0);
    write_valid_record(1, p1);

    off_t expected = 2 * static_cast<off_t>(sizeof(RecordHeader))
                   + static_cast<off_t>(p0.size())
                   + static_cast<off_t>(p1.size());
    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.last_good_end, expected);
}

TEST_F(ScanLogTest, TruncatedHeaderAtEndDoesNotCountAsGoodRecord) {
    write_valid_record(0, {'a', 'b'});
    const uint8_t partial_hdr[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    write_raw(partial_hdr, sizeof(partial_hdr));

    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.next_seq, 1u);
    EXPECT_EQ(result.last_good_end,
              static_cast<off_t>(sizeof(RecordHeader) + 2));
}

TEST_F(ScanLogTest, TruncatedPayloadAtEndDoesNotCountAsGoodRecord) {
    write_valid_record(0, {'x', 'y', 'z'});

    RecordHeader hdr{};
    hdr.sequence_number = 1;
    hdr.payload_length  = 10;
    hdr.crc32           = 0;
    write_raw(&hdr, sizeof(hdr));
    write_raw("123", 3);

    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.next_seq, 1u);
}

TEST_F(ScanLogTest, BadCrcAtEndDoesNotCountAsGoodRecord) {
    write_valid_record(0, {'g', 'o', 'o', 'd'});

    const std::vector<uint8_t> corrupt_payload = {'b', 'a', 'd'};
    RecordHeader hdr{};
    hdr.sequence_number = 1;
    hdr.payload_length  = static_cast<uint32_t>(corrupt_payload.size());
    hdr.crc32           = 0xDEADBEEF;
    write_raw(&hdr, sizeof(hdr));
    write_raw(corrupt_payload.data(), corrupt_payload.size());

    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.next_seq, 1u);
}

TEST_F(ScanLogTest, FileContainingOnlyJunkReturnsZeroSeqAndZeroEnd) {
    const uint8_t junk[] = {0xFF, 0xFF, 0xFF, 0xFF};
    write_raw(junk, sizeof(junk));

    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.next_seq,      0u);
    EXPECT_EQ(result.last_good_end, 0);
}

TEST_F(ScanLogTest, GoodRecordsPrecedingCorruptionArePreserved) {
    write_valid_record(0, {'a'});
    write_valid_record(1, {'b'});

    RecordHeader hdr{};
    hdr.sequence_number = 2;
    hdr.payload_length  = 5;
    hdr.crc32           = 0xBADC0FFE;
    write_raw(&hdr, sizeof(hdr));
    write_raw("hello", 5);

    LogScan result = scan_log(fd_);
    EXPECT_EQ(result.next_seq, 2u);
}

TEST_F(ScanLogTest, FetchNextSeqMatchesScanLogNextSeq) {
    write_valid_record(0, {1, 2});
    write_valid_record(1, {3, 4});

    EXPECT_EQ(fetch_next_seq(fd_), scan_log(fd_).next_seq);
}

TEST_F(ScanLogTest, FetchNextSeqOnEmptyFileReturnsZero) {
    EXPECT_EQ(fetch_next_seq(fd_), 0u);
}
