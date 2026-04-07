#pragma once
#include "partition_map.h"
#include "../../vendor/httplib.h"

class RouterServer {
public:
    RouterServer(PartitionMap& map, int port);
    void start();
    void stop();

private:
    void forward_produce(const httplib::Request& req, httplib::Response& res);
    void forward_consume(const httplib::Request& req, httplib::Response& res);
    void forward_tail(const httplib::Request& req, httplib::Response& res);
    void handle_topics(const httplib::Request& req, httplib::Response& res);

    PartitionMap& map_;
    httplib::Server server_;
    int port_;
};
