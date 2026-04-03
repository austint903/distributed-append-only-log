#include "udp_server/udp_server.h"
#include "http_server/http_server.h"
#include "topic_registry/topic_registry.h"
#include <thread>

int main() {
    TopicRegistry registry("test_logs");

    HttpServer http(registry);
    std::thread http_thread([&]{ http.start(); });

    UdpServer server(*registry.get_or_create("default").log);
    server.run();

    http.stop();
    http_thread.join();
    return 0;
}
