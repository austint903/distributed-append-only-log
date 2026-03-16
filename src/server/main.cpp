#include <cstdlib>
#include <iostream>
#include <cstring>
#include "append_log.h"
#include "../util/util.h"

int main() {
    char* working_directory = get_working_directory();
    std::cout << working_directory << "\n";
    free(working_directory);

    AppendLog log("src/logs/basic_append.log");

    const char* messages[] = {
        "test1",
        "test2",
        "test3",
    };

    for (const char* msg : messages) {
        auto* data = reinterpret_cast<const uint8_t*>(msg);
        log.append(std::span<const uint8_t>(data, strlen(msg)));
        std::cout << "appended: " << msg << "\n";
    }

    return 0;
}