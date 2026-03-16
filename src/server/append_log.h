#ifndef APPEND_LOG_H
#define APPEND_LOG_H

#include <cstdint>
#include <span>
#include <liburing.h>

struct RecordHeader {
    uint32_t payload_length;
    uint64_t sequence_number;
    uint32_t crc32;
} __attribute__((packed));

static_assert(sizeof(RecordHeader) == 16);

class AppendLog {
    int      fd_       = -1;
    io_uring ring_     = {};
    uint64_t next_seq_ = 0;

    static constexpr unsigned QUEUE_DEPTH = 1;

public:
    explicit AppendLog(const char* path);
    ~AppendLog();

    AppendLog(const AppendLog&)            = delete;
    AppendLog& operator=(const AppendLog&) = delete;

    void append(std::span<const uint8_t> payload);
};

#endif // APPEND_LOG_H
