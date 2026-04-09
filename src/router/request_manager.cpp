#include "request_manager.h"
#include <random>

RequestManager::RequestManager(PartitionMap& partition_map, HealthTracker& health_tracker)
    : partition_map_(partition_map), health_tracker_(health_tracker) {}

NodeInfo RequestManager::get_write_node(const std::string& topic) {
    return partition_map_.get_or_assign(topic);
}

std::optional<NodeInfo> RequestManager::get_read_node(const std::string& topic) {
    auto all = partition_map_.get_all_nodes(topic);
    auto healthy = health_tracker_.healthy_subset(all);

    if (healthy.empty()) return std::nullopt;

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, healthy.size() - 1);
    return healthy[dist(rng)];
}
