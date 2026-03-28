#include "log_reader.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

LogReader::LogReader(const AppendLog& append_log_, const char *path):append_log_(append_log_) {
    fd_ = open(path, O_RDONLY);
    if (fd_ < 0)
        throw std::runtime_error("open() failed");
}

LogReader::~LogReader() {
    if (fd_>0) {
        close(fd_);
    }
}

std::optional<std::vector<uint8_t>> LogReader::read(uint64_t sequenceNumber)  {
    auto fileOffset = append_log_.getOffset(sequenceNumber);
    if (!fileOffset) return std::nullopt;
    RecordHeader header{};
    ssize_t headerRead = pread(fd_, &header, sizeof(header), *fileOffset);
    if (headerRead < 0) throw std::runtime_error("header pread() failed");

    std::vector<uint8_t> buffer (header.payload_length);
    ssize_t bodyRead = pread(fd_, buffer.data(), header.payload_length, *fileOffset+sizeof(header));
    if (bodyRead < 0) throw std::runtime_error("body pread() failed");

    return buffer;
}