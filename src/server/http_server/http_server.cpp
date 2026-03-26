#include "http_server.h"

HttpServer::HttpServer(AppendLog &log) : log_(log) {
    http_server_.Post("/produce", [&](const httplib::Request &req, httplib::Response &res) {
        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t *>(req.body.data()), req.body.size());
        uint64_t seq = log_.append_and_seq(payload);
        res.set_content("{\"seq\":" + std::to_string(seq) + "}", "application/json");
    });

    http_server_.Get("/consume", [&](const httplib::Request &req, httplib::Response &res) {
    });
}

void HttpServer::start() {
    http_server_.listen("0.0.0.0", 8080);
}

void HttpServer::stop() {
    http_server_.stop();
}
