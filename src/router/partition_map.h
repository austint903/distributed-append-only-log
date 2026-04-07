#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

struct NodeInfo {
    std::string host;
    int port;
    std::string addr() const { return host + ":" + std::to_string(port); }
};

class PartitionMap {
public:
    PartitionMap(std::string persist_path, std::vector<NodeInfo> nodes);

    void load();
    void reconcile();
    NodeInfo get_or_assign(const std::string& topic);
    std::unordered_map<std::string, std::string> snapshot() const;

private:
    void flush_locked();
    NodeInfo pick_least_loaded_locked() const;

    mutable std::mutex mu_;
    std::unordered_map<std::string, std::string> assignments_;
    std::vector<NodeInfo> nodes_;
    std::string persist_path_;
};
