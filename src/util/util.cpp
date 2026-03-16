#include "util.h"

#include <unistd.h>
#include <vector>

static uint32_t crc32_table[256];
static bool crc32_table_ready = false;

static void crc32_init_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_table_ready = true;
}

uint32_t crc32_compute(const uint8_t* data, size_t len) {
    if (!crc32_table_ready) crc32_init_table();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    return crc ^ 0xFFFFFFFFu;
}

char* get_working_directory() {
    return getcwd(nullptr, 0); // malloc's the buffer; caller must free()
}

LogScan scan_log(int fd) {
    off_t offset = 0;
    LogScan result{0, 0};

    while (true) {
        RecordHeader hdr{};
        ssize_t n = pread(fd, &hdr, sizeof(hdr), offset);
        if (n == 0) break;
        if (n < (ssize_t)sizeof(hdr)) break;

        std::vector<uint8_t> payload(hdr.payload_length);
        ssize_t pn = pread(fd, payload.data(), hdr.payload_length, offset + sizeof(hdr));
        if (pn < (ssize_t)hdr.payload_length) break;

        if (crc32_compute(payload.data(), payload.size()) != hdr.crc32) break;

        offset += sizeof(hdr) + hdr.payload_length;
        result.next_seq     = hdr.sequence_number + 1;
        result.last_good_end = offset;
    }

    return result;
}

uint64_t fetch_next_seq(int fd) {
    return scan_log(fd).next_seq;
}

