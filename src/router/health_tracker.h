#pragma once
#include "partition_map.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class HealthTracker {
public:
    explicit HealthTracker(std::vector<NodeInfo> nodes);
    ~HealthTracker();

    bool is_healthy(const std::string& addr) const;
    std::vector<NodeInfo> healthy_subset(const std::vector<NodeInfo>& candidates) const;
    void start();
    void stop();

private:
    std::vector<NodeInfo> nodes_;
    mutable std::mutex mu_;
    std::unordered_map<std::string, bool> health_;
    std::atomic<bool> stop_{false};
    std::thread poll_thread_;

    void poll_loop();
};
