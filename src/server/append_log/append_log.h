#ifndef APPEND_LOG_H
#define APPEND_LOG_H

#include <cstdint>
#include <span>
#include <liburing.h>
#include <optional>

#include "../../util/util.h"
#include <unordered_map>
#include <mutex>

class AppendLog {
    int      fd_       = -1;
    io_uring ring_     = {};
    uint64_t next_seq_ = 0;
    off_t fileIndex = 0;
    mutable std::mutex mutex_;

    static constexpr unsigned QUEUE_DEPTH = 1;

    std::unordered_map<uint64_t, off_t>index_;
    void buildIndex();
    void addIndex(uint64_t sequenceNumber, off_t offset);

public:
    explicit AppendLog(const char* path);
    ~AppendLog();

    AppendLog(const AppendLog&)            = delete;
    AppendLog& operator=(const AppendLog&) = delete;

    uint64_t append_and_seq(std::span<const uint8_t> payload);
    void append(std::span<const uint8_t> payload) { append_and_seq(payload); }
    void recover();
    uint64_t next_seq() const { std::lock_guard<std::mutex> lock(mutex_); return next_seq_; }

    std::optional<off_t> getOffset(uint64_t sequenceNumber) const;
};

#endif // APPEND_LOG_H
