#pragma once
#include "partition_map.h"
#include "health_tracker.h"
#include "request_manager.h"
#include "../../vendor/httplib.h"

class RouterServer {
public:
    RouterServer(PartitionMap& map, std::vector<NodeInfo> all_nodes, int port);
    void start();
    void stop();

private:
    void forward_produce(const httplib::Request& req, httplib::Response& res);
    void forward_consume(const httplib::Request& req, httplib::Response& res);
    void forward_tail(const httplib::Request& req, httplib::Response& res);
    void handle_topics(const httplib::Request& req, httplib::Response& res);

    PartitionMap&    map_;
    HealthTracker    health_tracker_;
    RequestManager   request_manager_;
    httplib::Server  server_;
    int              port_;
};
