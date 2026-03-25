#include "server.h"

int main() {
    Server server("test_logs/basic_append.log");
    server.run();
    return 0;
}
