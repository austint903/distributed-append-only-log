#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>

#include "../src/server/append_log/append_log.h"
#include "../src/util/util.h"

// Read back the raw file and verify one record was written correctly.
static void test_append_writes_valid_record() {
    const char* path = "/tmp/test_append_log.bin";
    ::unlink(path); // start clean

    const std::string msg = "hello";
    const std::vector<uint8_t> payload(msg.begin(), msg.end());

    {
        AppendLog log(path);
        assert(log.next_seq() == 0);
        log.append(payload);
        assert(log.next_seq() == 1);
    } // destructor closes fd and exits io_uring

    // Open the file and read back the record manually.
    int fd = ::open(path, O_RDONLY);
    assert(fd >= 0);

    RecordHeader hdr{};
    ssize_t n = ::read(fd, &hdr, sizeof(hdr));
    assert(n == static_cast<ssize_t>(sizeof(hdr)));

    assert(hdr.sequence_number == 0);
    assert(hdr.payload_length  == static_cast<uint32_t>(payload.size()));

    std::vector<uint8_t> written_payload(hdr.payload_length);
    n = ::read(fd, written_payload.data(), hdr.payload_length);
    assert(n == static_cast<ssize_t>(hdr.payload_length));
    assert(written_payload == payload);

    uint32_t expected_crc = crc32_compute(payload.data(), payload.size());
    assert(hdr.crc32 == expected_crc);

    ::close(fd);
    ::unlink(path);

    printf("PASS test_append_writes_valid_record\n");
}

int main() {
    test_append_writes_valid_record();
    printf("All tests passed.\n");
    return 0;
}
