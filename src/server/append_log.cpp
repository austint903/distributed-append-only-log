#include "append_log.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

// crc32 hashing
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

static uint32_t crc32_compute(const uint8_t* data, size_t len) {
    if (!crc32_table_ready) crc32_init_table();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    return crc ^ 0xFFFFFFFFu;
}

AppendLog::AppendLog(const char* path) {
    fd_ = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ < 0)
        throw std::runtime_error("open() failed");

    if (io_uring_queue_init(QUEUE_DEPTH, &ring_, 0) != 0) {
        close(fd_);
        throw std::runtime_error("io_uring_queue_init() failed");
    }
}

AppendLog::~AppendLog() {
    io_uring_queue_exit(&ring_);
    if (fd_ >= 0) close(fd_);
}

void AppendLog::append(std::span<const uint8_t> payload) {
    RecordHeader hdr{};
    hdr.payload_length  = static_cast<uint32_t>(payload.size());
    hdr.sequence_number = next_seq_++;
    hdr.crc32           = crc32_compute(payload.data(), payload.size());

    std::vector<uint8_t> record(sizeof(RecordHeader) + payload.size());
    memcpy(record.data(), &hdr, sizeof(hdr));
    memcpy(record.data() + sizeof(hdr), payload.data(), payload.size());

    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe)
        throw std::runtime_error("submission queue full");

    io_uring_prep_write(sqe, fd_, record.data(),
                        static_cast<unsigned int>(record.size()), -1);

    if (io_uring_submit(&ring_) < 0)
        throw std::runtime_error("io_uring_submit() failed");

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&ring_, &cqe) < 0)
        throw std::runtime_error("io_uring_wait_cqe() failed");

    int result = cqe->res;
    io_uring_cqe_seen(&ring_, cqe);

    if (result < 0)
        throw std::runtime_error("write failed: " + std::to_string(-result));
}
