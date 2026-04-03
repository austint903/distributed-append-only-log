#ifndef TOPIC_REGISTRY_H
#define TOPIC_REGISTRY_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../append_log/append_log.h"
#include "../log_reader/log_reader.h"

class TopicRegistry {
public:
    struct TopicEntry {
        std::unique_ptr<AppendLog> log;
        std::unique_ptr<LogReader> reader;
    };

    explicit TopicRegistry(const std::string& data_dir);

    TopicEntry& get_or_create(const std::string& name);
    std::vector<std::string> list_topics() const;

private:
    std::string data_dir_;
    std::unordered_map<std::string, TopicEntry> topics_;
    mutable std::mutex mutex_;

    TopicEntry& create_locked(const std::string& name);
};

#endif // TOPIC_REGISTRY_H
