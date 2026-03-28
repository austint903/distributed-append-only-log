//
// Created by Austin Tan on 3/25/26.
//

#ifndef DISTRIBUTED_APPEND_ONLY_LOG_HTTP_SERVER_H
#define DISTRIBUTED_APPEND_ONLY_LOG_HTTP_SERVER_H
#include "../../../vendor/httplib.h"
#include "../append_log/append_log.h"
#include "../log_reader/log_reader.h"

class HttpServer {
    AppendLog& log_;
    LogReader& log_reader_;
    httplib::Server http_server_;
    public:
        explicit HttpServer(AppendLog& log, LogReader& log_reader);
        void start();
        void stop();
};
#endif //DISTRIBUTED_APPEND_ONLY_LOG_HTTP_SERVER_H