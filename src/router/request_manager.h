#pragma once
#include "partition_map.h"
#include "health_tracker.h"
#include <optional>
#include <string>

class RequestManager {
public:
    RequestManager(PartitionMap& partition_map, HealthTracker& health_tracker);

    NodeInfo get_write_node(const std::string& topic);
    std::optional<NodeInfo> get_read_node(const std::string& topic);

private:
    PartitionMap&  partition_map_;
    HealthTracker& health_tracker_;
};
