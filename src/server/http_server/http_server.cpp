#include "http_server.h"

HttpServer::HttpServer(TopicRegistry& registry) : registry_(registry) {
    http_server_.set_read_timeout(60, 0);
    http_server_.set_write_timeout(60, 0);

    http_server_.Post("/produce", [&](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_param("topic") || req.get_param_value("topic").empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"missing topic param\"}", "application/json");
            return;
        }
        const std::string topic = req.get_param_value("topic");
        auto& entry = registry_.get_or_create(topic);
        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(req.body.data()), req.body.size());
        uint64_t seq = entry.log->append_and_seq(payload);
        res.set_content("{\"seq\":" + std::to_string(seq) + "}", "application/json");
    });

    http_server_.Get("/consume", [&](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_param("topic") || req.get_param_value("topic").empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"missing topic param\"}", "application/json");
            return;
        }
        if (!req.has_param("offset")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing offset param\"}", "application/json");
            return;
        }
        const std::string topic = req.get_param_value("topic");
        uint64_t offset = std::stoull(req.get_param_value("offset"));
        auto& entry = registry_.get_or_create(topic);
        auto result = entry.reader->read(offset);
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

    http_server_.Get("/tail", [&](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_param("topic") || req.get_param_value("topic").empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"missing topic param\"}", "application/json");
            return;
        }
        if (!req.has_param("offset")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing offset param\"}", "application/json");
            return;
        }

        const std::string topic  = req.get_param_value("topic");
        const uint64_t    offset = std::stoull(req.get_param_value("offset"));
        const int timeout_ms = req.has_param("timeout_ms")
                                   ? std::stoi(req.get_param_value("timeout_ms"))
                                   : 30000;

        auto& entry = registry_.get_or_create(topic);

        //data exists
        auto result = entry.reader->read(offset);
        if (result.has_value()) {
            res.status = 200;
            res.set_header("X-Sequence-Number", std::to_string(offset));
            res.set_content(
                reinterpret_cast<const char*>(result->data()),
                result->size(),
                "application/octet-stream");
            return;
        }

        //block til data arrives or timeout
        bool arrived = entry.log->wait_for_seq(offset, std::chrono::milliseconds(timeout_ms));
        if (!arrived) {
            res.status = 408;
            res.set_content("{\"error\":\"timeout\"}", "application/json");
            return;
        }

        result = entry.reader->read(offset);
        if (!result.has_value()) {
            res.status = 500;
            res.set_content("{\"error\":\"internal error\"}", "application/json");
            return;
        }

        res.status = 200;
        res.set_header("X-Sequence-Number", std::to_string(offset));
        res.set_content(
            reinterpret_cast<const char*>(result->data()),
            result->size(),
            "application/octet-stream");
    });

    http_server_.Get("/topics", [&](const httplib::Request&, httplib::Response &res) {
        auto names = registry_.list_topics();
        std::string body = "{\"topics\":[";
        for (size_t i = 0; i < names.size(); i++) {
            if (i > 0) body += ",";
            body += "\"" + names[i] + "\"";
        }
        body += "]}";
        res.set_content(body, "application/json");
    });
}

void HttpServer::start() {
    http_server_.listen("0.0.0.0", 8080);
}

void HttpServer::stop() {
    http_server_.stop();
}
