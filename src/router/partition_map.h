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

struct ReplicaGroup {
    NodeInfo master;
    std::vector<NodeInfo> slaves;
    std::vector<NodeInfo> all() const {
        std::vector<NodeInfo> result = {master};
        result.insert(result.end(), slaves.begin(), slaves.end());
        return result;
    }
};

class PartitionMap {
public:
    PartitionMap(std::string persist_path, std::vector<ReplicaGroup> groups);

    void load();
    void reconcile();
    NodeInfo get_or_assign(const std::string& topic);
    std::vector<NodeInfo> get_all_nodes(const std::string& topic);
    std::unordered_map<std::string, std::string> snapshot() const;

private:
    void flush_locked();
    NodeInfo pick_least_loaded_locked() const;

    mutable std::mutex mu_;
    std::unordered_map<std::string, std::string> assignments_;
    std::vector<ReplicaGroup> groups_;
    std::unordered_map<std::string, int> group_index_by_master_;
    std::string persist_path_;
};
