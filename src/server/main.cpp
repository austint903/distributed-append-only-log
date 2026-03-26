#include "udp_server/udp_server.h"
#include "append_log/append_log.h"

int main() {
    AppendLog log("test_logs/basic_append.log");
    UdpServer server(log);
    server.run();
    return 0;
}
