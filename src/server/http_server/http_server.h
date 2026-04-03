//
// Created by Austin Tan on 3/25/26.
//

#ifndef DISTRIBUTED_APPEND_ONLY_LOG_HTTP_SERVER_H
#define DISTRIBUTED_APPEND_ONLY_LOG_HTTP_SERVER_H
#include "../../../vendor/httplib.h"
#include "../topic_registry/topic_registry.h"

class HttpServer {
    TopicRegistry& registry_;
    httplib::Server http_server_;
    public:
        explicit HttpServer(TopicRegistry& registry);
        void start();
        void stop();
};
#endif //DISTRIBUTED_APPEND_ONLY_LOG_HTTP_SERVER_H