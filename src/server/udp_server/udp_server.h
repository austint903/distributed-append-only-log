#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "../append_log/append_log.h"

class UdpServer {
    int       udp_fd_ = -1;
    AppendLog log_;

public:
    explicit UdpServer(const char* log_path);
    ~UdpServer();

    UdpServer(const UdpServer&)            = delete;
    UdpServer& operator=(const UdpServer&) = delete;

    void run();
};

#endif // UDP_SERVER_H
