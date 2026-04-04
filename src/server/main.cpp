#include "udp_server/udp_server.h"
#include "http_server/http_server.h"
#include "topic_registry/topic_registry.h"
#include <thread>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signal_handler(int signum) {
    std::cout<<"Received signal: "<<signum<<std::endl;
    running = false;
}
int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    TopicRegistry registry("test_logs");

    HttpServer http(registry);
    std::thread http_thread([&]{ http.start(); });

    UdpServer server(*registry.get_or_create("default").log);
    server.run(running);

    http.stop();
    http_thread.join();
    return 0;
}
