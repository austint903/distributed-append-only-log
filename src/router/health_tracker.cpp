#include "health_tracker.h"
#include "../../vendor/httplib.h"
#include <iostream>

HealthTracker::HealthTracker(std::vector<NodeInfo> nodes) : nodes_(std::move(nodes)) {
    for (const auto& n : nodes_) {
        health_[n.addr()] = true;
    }
}

HealthTracker::~HealthTracker() {
    stop();
}

void HealthTracker::start() {
    poll_thread_ = std::thread([this] { poll_loop(); });
}

void HealthTracker::stop() {
    stop_ = true;
    if (poll_thread_.joinable()) poll_thread_.join();
}

bool HealthTracker::is_healthy(const std::string& addr) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = health_.find(addr);
    return it != health_.end() && it->second;
}

std::vector<NodeInfo> HealthTracker::healthy_subset(const std::vector<NodeInfo>& candidates) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<NodeInfo> result;
    for (const auto& n : candidates) {
        auto it = health_.find(n.addr());
        if (it != health_.end() && it->second) {
            result.push_back(n);
        }
    }
    return result;
}

void HealthTracker::poll_loop() {
    while (!stop_) {
        for (const auto& node : nodes_) {
            if (stop_) break;
            httplib::Client cli(node.host, node.port);
            cli.set_connection_timeout(2, 0);
            cli.set_read_timeout(2, 0);
            auto res = cli.Get("/health");
            bool healthy = res && res->status == 200;
            {
                std::lock_guard<std::mutex> lock(mu_);
                bool prev = health_[node.addr()];
                if (prev != healthy) {
                    std::cout << "[health_tracker] " << node.addr()
                              << (healthy ? " is now healthy" : " is now unhealthy") << "\n";
                }
                health_[node.addr()] = healthy;
            }
        }
        for (int i = 0; i < 50 && !stop_; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
