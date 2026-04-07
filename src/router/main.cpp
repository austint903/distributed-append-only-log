#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include "partition_map.h"
#include "router_server.h"

static std::vector<NodeInfo> parse_nodes(const std::string& nodes_str) {
    std::vector<NodeInfo> nodes;
    size_t pos = 0;
    while (pos < nodes_str.size()) {
        auto comma = nodes_str.find(',', pos);
        std::string token = (comma == std::string::npos)
            ? nodes_str.substr(pos)
            : nodes_str.substr(pos, comma - pos);
        auto colon = token.rfind(':');
        if (colon != std::string::npos) {
            nodes.push_back({token.substr(0, colon), std::stoi(token.substr(colon + 1))});
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return nodes;
}

int main() {
    const char* nodes_env    = std::getenv("NODES");
    const char* port_env     = std::getenv("PORT");
    const char* data_dir_env = std::getenv("DATA_DIR");

    std::string nodes_str = nodes_env    ? nodes_env    : "localhost:8080,localhost:8081,localhost:8083";
    int         port      = port_env     ? std::stoi(port_env) : 9090;
    std::string data_dir  = data_dir_env ? data_dir_env : "data/router";

    auto nodes = parse_nodes(nodes_str);
    if (nodes.empty()) {
        std::cerr << "Error: no valid nodes in NODES env var\n";
        return 1;
    }

    std::string persist_path = data_dir + "/partitions.json";

    PartitionMap map(persist_path, std::move(nodes));
    map.load();
    map.reconcile();

    std::cout << "router listening on port " << port << "\n";
    std::cout << "partition map: " << persist_path << "\n";

    RouterServer router(map, port);
    router.start();
    return 0;
}
