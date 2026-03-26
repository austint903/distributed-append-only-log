#include "udp_server/udp_server.h"

int main() {
    UdpServer server("test_logs/basic_append.log");
    server.run();
    return 0;
}
