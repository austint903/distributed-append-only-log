#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include "partition_map.h"
#include "router_server.h"

static std::vector<ReplicaGroup> parse_replica_groups(const std::string& s) {
    std::vector<ReplicaGroup> groups;

    auto parse_node = [](const std::string& token) -> NodeInfo {
        auto colon = token.rfind(':');
        if (colon == std::string::npos) return {token, 8080};
        return {token.substr(0, colon), std::stoi(token.substr(colon + 1))};
    };

    size_t gpos = 0;
    while (gpos < s.size()) {
        auto semi = s.find(';', gpos);
        std::string group_str = (semi == std::string::npos)
            ? s.substr(gpos)
            : s.substr(gpos, semi - gpos);

        ReplicaGroup group;
        bool first = true;
        size_t npos = 0;
        while (npos < group_str.size()) {
            auto comma = group_str.find(',', npos);
            std::string token = (comma == std::string::npos)
                ? group_str.substr(npos)
                : group_str.substr(npos, comma - npos);

            if (!token.empty()) {
                if (first) { group.master = parse_node(token); first = false; }
                else        { group.slaves.push_back(parse_node(token)); }
            }

            if (comma == std::string::npos) break;
            npos = comma + 1;
        }

        if (!first) groups.push_back(std::move(group));
        if (semi == std::string::npos) break;
        gpos = semi + 1;
    }
    return groups;
}

static std::vector<NodeInfo> all_nodes(const std::vector<ReplicaGroup>& groups) {
    std::vector<NodeInfo> nodes;
    for (auto& g : groups) {
        nodes.push_back(g.master);
        for (auto& s : g.slaves) nodes.push_back(s);
    }
    return nodes;
}

int main() {
    const char* groups_env   = std::getenv("REPLICA_GROUPS");
    const char* port_env     = std::getenv("PORT");
    const char* data_dir_env = std::getenv("DATA_DIR");

    std::string groups_str = groups_env   ? groups_env   : "localhost:8080;localhost:8081;localhost:8083";
    int         port       = port_env     ? std::stoi(port_env) : 9090;
    std::string data_dir   = data_dir_env ? data_dir_env : "data/router";

    auto groups = parse_replica_groups(groups_str);
    if (groups.empty()) {
        std::cerr << "Error: no valid groups in REPLICA_GROUPS env var\n";
        return 1;
    }

    std::string persist_path = data_dir + "/partitions.json";

    PartitionMap map(persist_path, groups);
    map.load();
    map.reconcile();

    std::cout << "router listening on port " << port << "\n";
    std::cout << "partition map: " << persist_path << "\n";

    RouterServer router(map, all_nodes(groups), port);
    router.start();
    return 0;
}
