#include "router_server.h"

RouterServer::RouterServer(PartitionMap& map, int port)
    : map_(map), port_(port) {
    server_.set_read_timeout(120, 0);
    server_.set_write_timeout(120, 0);

    server_.Post("/produce", [&](const httplib::Request& req, httplib::Response& res) {
        forward_produce(req, res);
    });
    server_.Get("/consume", [&](const httplib::Request& req, httplib::Response& res) {
        forward_consume(req, res);
    });
    server_.Get("/tail", [&](const httplib::Request& req, httplib::Response& res) {
        forward_tail(req, res);
    });
    server_.Get("/topics", [&](const httplib::Request& req, httplib::Response& res) {
        handle_topics(req, res);
    });
}

void RouterServer::start() {
    server_.listen("0.0.0.0", port_);
}

void RouterServer::stop() {
    server_.stop();
}

void RouterServer::forward_produce(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("topic") || req.get_param_value("topic").empty()) {
        res.status = 400;
        res.set_content("{\"error\":\"missing topic param\"}", "application/json");
        return;
    }
    auto topic = req.get_param_value("topic");
    auto node = map_.get_or_assign(topic);

    httplib::Client cli(node.host, node.port);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);

    auto ct = req.get_header_value("Content-Type");
    auto backend_res = cli.Post("/produce?topic=" + topic, req.body, ct.empty() ? "text/plain" : ct);
    if (!backend_res) {
        res.status = 502;
        res.set_content("{\"error\":\"backend unreachable\"}", "application/json");
        return;
    }
    res.status = backend_res->status;
    res.set_content(backend_res->body, "application/json");
}

void RouterServer::forward_consume(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("topic") || req.get_param_value("topic").empty()) {
        res.status = 400;
        res.set_content("{\"error\":\"missing topic param\"}", "application/json");
        return;
    }
    auto topic = req.get_param_value("topic");
    auto node = map_.get_or_assign(topic);

    httplib::Client cli(node.host, node.port);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(10, 0);

    std::string url = "/consume?topic=" + topic;
    if (req.has_param("offset")) url += "&offset=" + req.get_param_value("offset");

    auto backend_res = cli.Get(url);
    if (!backend_res) {
        res.status = 502;
        res.set_content("{\"error\":\"backend unreachable\"}", "application/json");
        return;
    }
    res.status = backend_res->status;
    auto ct = backend_res->get_header_value("Content-Type");
    res.set_content(backend_res->body, ct.empty() ? "application/octet-stream" : ct);
}

void RouterServer::forward_tail(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("topic") || req.get_param_value("topic").empty()) {
        res.status = 400;
        res.set_content("{\"error\":\"missing topic param\"}", "application/json");
        return;
    }
    auto topic = req.get_param_value("topic");
    auto node = map_.get_or_assign(topic);

    int timeout_ms = req.has_param("timeout_ms")
        ? std::stoi(req.get_param_value("timeout_ms"))
        : 30000;

    httplib::Client cli(node.host, node.port);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(timeout_ms / 1000 + 10, 0);

    std::string url = "/tail?topic=" + topic;
    if (req.has_param("offset")) url += "&offset=" + req.get_param_value("offset");
    url += "&timeout_ms=" + std::to_string(timeout_ms);

    auto backend_res = cli.Get(url);
    if (!backend_res) {
        res.status = 502;
        res.set_content("{\"error\":\"backend unreachable\"}", "application/json");
        return;
    }
    res.status = backend_res->status;
    auto seq = backend_res->get_header_value("X-Sequence-Number");
    if (!seq.empty()) res.set_header("X-Sequence-Number", seq);
    auto ct = backend_res->get_header_value("Content-Type");
    res.set_content(backend_res->body, ct.empty() ? "application/octet-stream" : ct);
}

void RouterServer::handle_topics(const httplib::Request&, httplib::Response& res) {
    auto assignments = map_.snapshot();
    std::string body = "{\"topics\":[";
    bool first = true;
    for (auto& [topic, addr] : assignments) {
        if (!first) body += ",";
        body += "{\"topic\":\"" + topic + "\",\"node\":\"" + addr + "\"}";
        first = false;
    }
    body += "]}";
    res.set_content(body, "application/json");
}
