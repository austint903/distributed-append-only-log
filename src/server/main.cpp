#include "udp_server/udp_server.h"
#include "append_log/append_log.h"
#include "log_reader/log_reader.h"
#include "http_server/http_server.h"
#include <thread>

int main() {
    AppendLog log("test_logs/basic_append.log");
    LogReader reader(log, "test_logs/basic_append.log");

    HttpServer http(log, reader);
    std::thread http_thread([&]{ http.start(); });

    UdpServer server(log);
    server.run();

    http.stop();
    http_thread.join();
    return 0;
}
