#include "http_server.h"

HttpServer::HttpServer(AppendLog &log, LogReader& log_reader) : log_(log), log_reader_(log_reader) {
    http_server_.Post("/produce", [&](const httplib::Request &req, httplib::Response &res) {
        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t *>(req.body.data()), req.body.size());
        uint64_t seq = log_.append_and_seq(payload);
        res.set_content("{\"seq\":" + std::to_string(seq) + "}", "application/json");
    });

    http_server_.Get("/consume", [&](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_param("seq")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing seq param\"}", "application/json");
            return;
        }
        uint64_t seq = std::stoull(req.get_param_value("seq"));
        auto result = log_reader_.read(seq);
        if (!result.has_value()) {
            res.status = 404;
            res.set_content("{\"error\":\"not found\"}", "application/json");
            return;
        }
        res.status = 200;
        res.set_content(
            reinterpret_cast<const char*>(result->data()),
            result->size(),
            "application/octet-stream"
        );
    });
}

void HttpServer::start() {
    http_server_.listen("0.0.0.0", 8080);
}

void HttpServer::stop() {
    http_server_.stop();
}
