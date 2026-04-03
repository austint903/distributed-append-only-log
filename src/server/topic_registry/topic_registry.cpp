#include "topic_registry.h"
#include <filesystem>
#include <algorithm>

TopicRegistry::TopicRegistry(const std::string& data_dir) : data_dir_(data_dir) {
    std::filesystem::create_directories(data_dir_);

    for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
        const std::string stem = entry.path().stem().string();
        if (entry.path().extension() == ".log" && stem.size() > 4 && stem.substr(stem.size() - 4) == "_log") {
            const std::string name = stem.substr(0, stem.size() - 4);
            create_locked(name);
        }
    }
}

TopicRegistry::TopicEntry& TopicRegistry::get_or_create(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = topics_.find(name);
    if (it != topics_.end()) {
        return it->second;
    }
    return create_locked(name);
}

std::vector<std::string> TopicRegistry::list_topics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(topics_.size());
    for (const auto& [name, _] : topics_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

TopicRegistry::TopicEntry& TopicRegistry::create_locked(const std::string& name) {
    std::string path = data_dir_ + "/" + name + "_log.log";
    auto log = std::make_unique<AppendLog>(path.c_str());
    auto reader = std::make_unique<LogReader>(*log, path.c_str());
    auto [it, _] = topics_.emplace(name, TopicEntry{std::move(log), std::move(reader)});
    return it->second;
}
