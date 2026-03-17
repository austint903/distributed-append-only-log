#ifndef SERVER_H
#define SERVER_H

#include "append_log.h"

class Server {
    int       udp_fd_ = -1;
    AppendLog log_;

public:
    explicit Server(const char* log_path);
    ~Server();

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    void run();
};

#endif // SERVER_H
