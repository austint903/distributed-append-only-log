#ifndef APPEND_LOG_H
#define APPEND_LOG_H

#include <cstdint>
#include <span>
#include <liburing.h>
#include "../../util/util.h"

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

    uint64_t append_and_seq(std::span<const uint8_t> payload);
    void append(std::span<const uint8_t> payload) { append_and_seq(payload); }
    void recover();
    uint64_t next_seq() const { return next_seq_; }
};

#endif // APPEND_LOG_H
