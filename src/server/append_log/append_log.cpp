#include "append_log.h"
#include "../../util/util.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

AppendLog::AppendLog(const char* path) {
    fd_ = open(path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd_ < 0)
        throw std::runtime_error("open() failed");

    if (io_uring_queue_init(QUEUE_DEPTH, &ring_, 0) != 0) {
        close(fd_);
        throw std::runtime_error("io_uring_queue_init() failed");
    }

    recover();
}

void AppendLog::recover() {
    auto [next_seq, last_good_end] = scan_log(fd_);
    next_seq_ = next_seq;

    if (ftruncate(fd_, last_good_end) != 0)
        throw std::runtime_error("ftruncate() failed during recovery");
    fileIndex = last_good_end;
}

AppendLog::~AppendLog() {
    io_uring_queue_exit(&ring_);
    if (fd_ >= 0) close(fd_);
}

void AppendLog::addIndex(uint64_t sequenceNumber, off_t offset) {
    index_[sequenceNumber] = offset;
}

void AppendLog::buildIndex() {

}

uint64_t AppendLog::append_and_seq(std::span<const uint8_t> payload) {
    RecordHeader hdr{};
    hdr.payload_length  = static_cast<uint32_t>(payload.size());
    hdr.sequence_number = next_seq_;
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

    //addIndex here
    addIndex(next_seq_, fileIndex);
    fileIndex+=record.size();
    return next_seq_++;
}

std::optional<off_t>AppendLog::getOffset(uint64_t sequenceNumber) {
    auto it = index_.find(sequenceNumber);
    if (it == index_.end())return std::nullopt;
    return it->second;
}
