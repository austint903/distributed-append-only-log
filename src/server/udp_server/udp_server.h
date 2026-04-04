#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "../append_log/append_log.h"
#include <atomic>

class UdpServer {
    int        udp_fd_ = -1;
    AppendLog& log_;

public:
    explicit UdpServer(AppendLog& log);
    ~UdpServer();

    UdpServer(const UdpServer&)            = delete;
    UdpServer& operator=(const UdpServer&) = delete;

    void run(std::atomic<bool>& running);
};

#endif // UDP_SERVER_H
