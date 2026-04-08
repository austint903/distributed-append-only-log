#include "node_manager.h"
#include "../../../vendor/httplib.h"
#include <cstdlib>
#include <iostream>
#include <span>

static std::pair<std::string, int> parse_master_url(const std::string& url) {
    std::string s = url;
    if (s.substr(0, 7) == "http://") s = s.substr(7);
    auto colon = s.rfind(':');
    if (colon == std::string::npos) return {s, 8080};
    return {s.substr(0, colon), std::stoi(s.substr(colon + 1))};
}

static std::vector<std::string> parse_topics_json(const std::string& json) {
    std::vector<std::string> topics;
    auto start = json.find('[');
    auto end   = json.rfind(']');
    if (start == std::string::npos || end == std::string::npos) return topics;
    std::string arr = json.substr(start + 1, end - start - 1);
    size_t pos = 0;
    while ((pos = arr.find('"', pos)) != std::string::npos) {
        size_t close = arr.find('"', pos + 1);
        if (close == std::string::npos) break;
        topics.push_back(arr.substr(pos + 1, close - pos - 1));
        pos = close + 1;
    }
    return topics;
}

NodeManager::NodeManager(TopicRegistry& registry) : registry_(registry) {
    const char* role_env = std::getenv("ROLE");
    role_ = (role_env && std::string(role_env) == "slave") ? Role::Slave : Role::Master;

    if (role_ == Role::Slave) {
        const char* master_env = std::getenv("MASTER_URL");
        if (!master_env) {
            std::cerr << "[node_manager] ROLE=slave but MASTER_URL not set\n";
            role_ = Role::Master;
        } else {
            auto [host, port] = parse_master_url(master_env);
            master_host_ = host;
            master_port_ = port;
        }
    }
}

NodeManager::~NodeManager() {
    stop();
}

void NodeManager::start() {
    if (role_ == Role::Master) return;
    discovery_thread_ = std::thread([this] { run_discovery(); });
}

void NodeManager::stop() {
    stop_ = true;
    if (discovery_thread_.joinable()) discovery_thread_.join();
    std::lock_guard<std::mutex> lock(topics_mutex_);
    for (auto& [topic, t] : topic_threads_) {
        if (t.joinable()) t.join();
    }
}

void NodeManager::run_discovery() {
    httplib::Client client(master_host_, master_port_);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(2, 0);

    while (!stop_) {
        auto res = client.Get("/topics");
        if (res && res->status == 200) {
            for (const auto& topic : parse_topics_json(res->body)) {
                start_tailing(topic);
            }
        }
        for (int i = 0; i < 50 && !stop_; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void NodeManager::start_tailing(const std::string& topic) {
    std::lock_guard<std::mutex> lock(topics_mutex_);
    if (topic_threads_.count(topic)) return;
    topic_threads_.emplace(topic, std::thread([this, topic] { tail_topic(topic); }));
}

void NodeManager::tail_topic(const std::string& topic) {
    httplib::Client client(master_host_, master_port_);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(10, 0);

    uint64_t offset = registry_.get_or_create(topic).log->next_seq();

    while (!stop_) {
        std::string path = "/tail?topic=" + topic +
                           "&offset=" + std::to_string(offset) +
                           "&timeout_ms=5000";
        auto res = client.Get(path);
        if (!res) {
            for (int i = 0; i < 20 && !stop_; i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (res->status == 200) {
            auto payload = std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(res->body.data()), res->body.size());
            registry_.get_or_create(topic).log->append_and_seq(payload);
            offset++;
        } else if (res->status != 408) {
            for (int i = 0; i < 20 && !stop_; i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
