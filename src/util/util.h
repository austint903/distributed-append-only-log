#ifndef DISTRIBUTED_APPEND_ONLY_LOG_UTIL_H
#define DISTRIBUTED_APPEND_ONLY_LOG_UTIL_H

#include <cstdint>
#include <sys/types.h>

struct RecordHeader {
    uint32_t payload_length;
    uint64_t sequence_number;
    uint32_t crc32;
} __attribute__((packed));

static_assert(sizeof(RecordHeader) == 16);

struct LogScan {
    uint64_t next_seq;
    off_t    last_good_end;
};

char*    get_working_directory();
uint32_t crc32_compute(const uint8_t* data, size_t len);
LogScan  scan_log(int fd);
uint64_t fetch_next_seq(int fd);   // convenience wrapper around scan_log

#endif