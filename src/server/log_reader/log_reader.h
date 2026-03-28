#ifndef LOG_READER
#define LOG_READER
#include <cstdint>
#include <optional>
#include <vector>
#include "../append_log/append_log.h"
class LogReader {
    const AppendLog& append_log_;
    int fd_;

    public:
        explicit LogReader(const AppendLog& append_log_, const char* path);
        ~LogReader();
        std::optional<std::vector<uint8_t>> read(uint64_t sequenceNumber);
};


#endif