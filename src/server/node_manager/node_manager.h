#ifndef NODE_MANAGER_H
#define NODE_MANAGER_H

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include "../topic_registry/topic_registry.h"

class NodeManager {
public:
    explicit NodeManager(TopicRegistry& registry);
    ~NodeManager();

    bool is_master() const { return role_ == Role::Master; }
    void start();
    void stop();

private:
    enum class Role { Master, Slave };

    Role        role_;
    std::string master_host_;
    int         master_port_ = 8080;
    TopicRegistry& registry_;

    std::atomic<bool> stop_{false};
    std::thread       discovery_thread_;
    std::mutex        topics_mutex_;
    std::map<std::string, std::thread> topic_threads_;

    void run_discovery();
    void start_tailing(const std::string& topic);
    void tail_topic(const std::string& topic);
};

#endif // NODE_MANAGER_H
