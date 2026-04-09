#include "partition_map.h"
#include "../../vendor/httplib.h"
#include <fstream>
#include <filesystem>

static std::vector<std::string> parse_topics_json(const std::string& body) {
    std::vector<std::string> result;
    auto arr_start = body.find('[');
    auto arr_end   = body.find(']');
    if (arr_start == std::string::npos || arr_end == std::string::npos) return result;
    std::string arr = body.substr(arr_start + 1, arr_end - arr_start - 1);
    size_t pos = 0;
    while (pos < arr.size()) {
        auto q1 = arr.find('"', pos);
        if (q1 == std::string::npos) break;
        auto q2 = arr.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        result.push_back(arr.substr(q1 + 1, q2 - q1 - 1));
        pos = q2 + 1;
    }
    return result;
}

static std::unordered_map<std::string, std::string> parse_assignments_json(const std::string& body) {
    std::unordered_map<std::string, std::string> result;
    auto start = body.find("\"assignments\"");
    if (start == std::string::npos) return result;
    auto brace = body.find('{', start + 13);
    if (brace == std::string::npos) return result;
    auto end = body.find('}', brace + 1);
    if (end == std::string::npos) return result;
    std::string inner = body.substr(brace + 1, end - brace - 1);
    size_t pos = 0;
    while (pos < inner.size()) {
        auto q1 = inner.find('"', pos);
        if (q1 == std::string::npos) break;
        auto q2 = inner.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string key = inner.substr(q1 + 1, q2 - q1 - 1);
        auto colon = inner.find(':', q2 + 1);
        if (colon == std::string::npos) break;
        auto q3 = inner.find('"', colon + 1);
        if (q3 == std::string::npos) break;
        auto q4 = inner.find('"', q3 + 1);
        if (q4 == std::string::npos) break;
        result[key] = inner.substr(q3 + 1, q4 - q3 - 1);
        pos = q4 + 1;
    }
    return result;
}

static std::string build_assignments_json(const std::unordered_map<std::string, std::string>& assignments) {
    std::string body = "{\n  \"assignments\": {\n";
    bool first = true;
    for (auto& [topic, addr] : assignments) {
        if (!first) body += ",\n";
        body += "    \"" + topic + "\": \"" + addr + "\"";
        first = false;
    }
    body += "\n  }\n}\n";
    return body;
}

PartitionMap::PartitionMap(std::string persist_path, std::vector<ReplicaGroup> groups)
    : persist_path_(std::move(persist_path)), groups_(std::move(groups)) {
    for (int i = 0; i < (int)groups_.size(); i++) {
        group_index_by_master_[groups_[i].master.addr()] = i;
    }
}

void PartitionMap::load() {
    std::ifstream f(persist_path_);
    if (!f.is_open()) return;
    std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::lock_guard<std::mutex> lock(mu_);
    assignments_ = parse_assignments_json(body);
}

void PartitionMap::reconcile() {
    std::unordered_map<std::string, std::string> discovered;
    for (auto& group : groups_) {
        auto& node = group.master;
        httplib::Client cli(node.host, node.port);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(2, 0);
        auto res = cli.Get("/topics");
        if (!res || res->status != 200) continue;
        for (auto& topic : parse_topics_json(res->body)) {
            if (discovered.find(topic) == discovered.end()) {
                discovered[topic] = node.addr();
            }
        }
    }
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& [topic, addr] : discovered) {
        if (assignments_.find(topic) == assignments_.end()) {
            assignments_[topic] = addr;
        }
    }
    flush_locked();
}

NodeInfo PartitionMap::get_or_assign(const std::string& topic) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = assignments_.find(topic);
    if (it != assignments_.end()) {
        auto& addr = it->second;
        auto colon = addr.rfind(':');
        return {addr.substr(0, colon), std::stoi(addr.substr(colon + 1))};
    }
    auto node = pick_least_loaded_locked();
    assignments_[topic] = node.addr();
    flush_locked();
    return node;
}

std::vector<NodeInfo> PartitionMap::get_all_nodes(const std::string& topic) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = assignments_.find(topic);
    if (it == assignments_.end()) return {};
    auto idx_it = group_index_by_master_.find(it->second);
    if (idx_it == group_index_by_master_.end()) return {};
    return groups_[idx_it->second].all();
}

std::unordered_map<std::string, std::string> PartitionMap::snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return assignments_;
}

void PartitionMap::flush_locked() {
    std::filesystem::create_directories(std::filesystem::path(persist_path_).parent_path());
    std::string tmp = persist_path_ + ".tmp";
    {
        std::ofstream f(tmp);
        if (!f.is_open()) return;
        f << build_assignments_json(assignments_);
    }
    std::filesystem::rename(tmp, persist_path_);
}

NodeInfo PartitionMap::pick_least_loaded_locked() const {
    std::unordered_map<std::string, int> counts;
    for (auto& g : groups_) counts[g.master.addr()] = 0;
    for (auto& [topic, addr] : assignments_) counts[addr]++;

    const NodeInfo* best = &groups_[0].master;
    int best_count = counts[groups_[0].master.addr()];
    for (auto& g : groups_) {
        int c = counts[g.master.addr()];
        if (c < best_count) {
            best_count = c;
            best = &g.master;
        }
    }
    return *best;
}
