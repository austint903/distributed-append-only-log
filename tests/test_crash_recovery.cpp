#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>

#include "../src/server/append_log.h"
#include "../src/util/util.h"

static const char* PATH = "/tmp/test_crash_recovery.bin";

static void raw_append(int fd, const void* data, size_t len) {
    ssize_t n = ::write(fd, data, len);
    assert(n == static_cast<ssize_t>(len));
}

static void test_recover_truncated_header() {
    ::unlink(PATH);

    {
        AppendLog log(PATH);
        const std::string a = "alpha", b = "beta";
        log.append(std::vector<uint8_t>(a.begin(), a.end()));
        log.append(std::vector<uint8_t>(b.begin(), b.end()));
    }

    {
        int fd = ::open(PATH, O_WRONLY | O_APPEND, 0644);
        assert(fd >= 0);
        uint8_t junk[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        raw_append(fd, junk, sizeof(junk));
        ::close(fd);
    }

    {
        AppendLog log(PATH);
        assert(log.next_seq() == 2);

        const std::string c = "gamma";
        log.append(std::vector<uint8_t>(c.begin(), c.end()));
        assert(log.next_seq() == 3);
    }

    {
        int fd = ::open(PATH, O_RDONLY);
        assert(fd >= 0);
        const std::vector<std::string> expected = {"alpha", "beta", "gamma"};
        off_t offset = 0;
        for (uint64_t seq = 0; seq < 3; ++seq) {
            RecordHeader hdr{};
            ssize_t n = ::pread(fd, &hdr, sizeof(hdr), offset);
            assert(n == static_cast<ssize_t>(sizeof(hdr)));
            assert(hdr.sequence_number == seq);
            assert(hdr.payload_length  == expected[seq].size());

            std::vector<uint8_t> payload(hdr.payload_length);
            n = ::pread(fd, payload.data(), hdr.payload_length, offset + sizeof(hdr));
            assert(n == static_cast<ssize_t>(hdr.payload_length));
            assert(std::string(payload.begin(), payload.end()) == expected[seq]);
            assert(hdr.crc32 == crc32_compute(payload.data(), payload.size()));

            offset += sizeof(hdr) + hdr.payload_length;
        }
        uint8_t extra;
        assert(::pread(fd, &extra, 1, offset) == 0);
        ::close(fd);
    }

    ::unlink(PATH);
    printf("PASS test_recover_truncated_header\n");
}

static void test_recover_truncated_payload() {
    ::unlink(PATH);

    {
        AppendLog log(PATH);
        const std::string a = "first", b = "second";
        log.append(std::vector<uint8_t>(a.begin(), a.end()));
        log.append(std::vector<uint8_t>(b.begin(), b.end()));
    }

    {
        int fd = ::open(PATH, O_WRONLY | O_APPEND, 0644);
        assert(fd >= 0);

        const std::string partial_payload = "12345";
        RecordHeader hdr{};
        hdr.sequence_number = 2;
        hdr.payload_length  = 10;
        hdr.crc32           = 0;

        raw_append(fd, &hdr, sizeof(hdr));
        raw_append(fd, partial_payload.data(), partial_payload.size());
        ::close(fd);
    }

    {
        AppendLog log(PATH);
        assert(log.next_seq() == 2);
    }

    ::unlink(PATH);
    printf("PASS test_recover_truncated_payload\n");
}

static void test_recover_bad_crc() {
    ::unlink(PATH);

    {
        AppendLog log(PATH);
        const std::string a = "valid";
        log.append(std::vector<uint8_t>(a.begin(), a.end()));
    }

    {
        int fd = ::open(PATH, O_WRONLY | O_APPEND, 0644);
        assert(fd >= 0);

        const std::string bad = "corrupt";
        RecordHeader hdr{};
        hdr.sequence_number = 1;
        hdr.payload_length  = static_cast<uint32_t>(bad.size());
        hdr.crc32           = 0xDEADBEEF;

        raw_append(fd, &hdr, sizeof(hdr));
        raw_append(fd, bad.data(), bad.size());
        ::close(fd);
    }

    {
        AppendLog log(PATH);
        assert(log.next_seq() == 1);
    }

    ::unlink(PATH);
    printf("PASS test_recover_bad_crc\n");
}

static void test_recover_empty_file() {
    ::unlink(PATH);

    int fd = ::open(PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    ::close(fd);

    {
        AppendLog log(PATH);
        assert(log.next_seq() == 0);

        const std::string msg = "after empty recovery";
        log.append(std::vector<uint8_t>(msg.begin(), msg.end()));
        assert(log.next_seq() == 1);
    }

    ::unlink(PATH);
    printf("PASS test_recover_empty_file\n");
}

int main() {
    test_recover_truncated_header();
    test_recover_truncated_payload();
    test_recover_bad_crc();
    test_recover_empty_file();
    printf("All crash recovery tests passed.\n");
    return 0;
}
