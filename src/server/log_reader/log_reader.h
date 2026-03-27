#ifndef LOG_READER
#define LOG_READER
#include <cstdint>
#include <optional>
#include <vector>

class LogReader {
    int fd_;
    public:
        explicit LogReader(const char* path);
        ~LogReader();
        std::optional<std::vector<uint8_t>> read();
};


#endif