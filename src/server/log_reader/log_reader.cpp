#include "log_reader.h"
#include <unistd.h>

LogReader::LogReader(const char *path) {

}

LogReader::~LogReader() {
    if (fd_>0) {
        close(fd_);
    }
}

std::optional<std::vector<uint8_t>> LogReader::read() {
    return std::nullopt;
}