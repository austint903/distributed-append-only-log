#include <gtest/gtest.h>
#include <filesystem>
#include <optional>

#include "../src/router/partition_map.h"
#include "../src/router/health_tracker.h"
#include "../src/router/request_manager.h"

// HealthTracker initialises all constructor nodes to healthy=true without
// starting the poll thread, so health state is fully deterministic here.
//
// Nodes registered in HealthTracker are "known-healthy".
// Nodes NOT registered are unknown → healthy_subset filters them out.
// This lets us simulate unhealthy nodes without any real HTTP calls.

static const NodeInfo kMaster{"10.1.0.1", 8080};
static const NodeInfo kSlave {"10.1.0.2", 8080};

class RequestManagerTest : public ::testing::Test {
protected:
    std::string path_;

    void SetUp() override {
        path_ = std::string("/tmp/rm_test_") +
                ::testing::UnitTest::GetInstance()->current_test_info()->name() +
                "/partitions.json";
        std::filesystem::create_directories(
            std::filesystem::path(path_).parent_path());
    }

    void TearDown() override {
        std::filesystem::remove_all(
            std::filesystem::path(path_).parent_path());
    }
};

// ─── get_write_node ────────────────────────────────────────────────────────

TEST_F(RequestManagerTest, GetWriteNode_NewTopic_ReturnsMasterNode) {
    PartitionMap  pm(path_, {ReplicaGroup{kMaster, {kSlave}}});
    HealthTracker ht({kMaster, kSlave});
    RequestManager rm(pm, ht);

    NodeInfo node = rm.get_write_node("orders");

    // get_write_node always delegates to PartitionMap::get_or_assign which
    // returns the master for the group.
    EXPECT_EQ(node.addr(), kMaster.addr());
}

TEST_F(RequestManagerTest, GetWriteNode_ExistingTopic_ReturnsSameNodeEachTime) {
    PartitionMap  pm(path_, {ReplicaGroup{kMaster, {kSlave}}});
    HealthTracker ht({kMaster, kSlave});
    RequestManager rm(pm, ht);

    NodeInfo first  = rm.get_write_node("events");
    NodeInfo second = rm.get_write_node("events");

    EXPECT_EQ(first.addr(), second.addr());
}

// ─── get_read_node ─────────────────────────────────────────────────────────

TEST_F(RequestManagerTest, GetReadNode_TopicNotYetAssigned_ReturnsNullopt) {
    // No call to get_write_node / get_or_assign → no assignment exists.
    // get_all_nodes returns {} → healthy_subset returns {} → nullopt.
    PartitionMap  pm(path_, {ReplicaGroup{kMaster, {kSlave}}});
    HealthTracker ht({kMaster, kSlave});
    RequestManager rm(pm, ht);

    auto result = rm.get_read_node("never-produced-to");

    EXPECT_FALSE(result.has_value());
}

TEST_F(RequestManagerTest, GetReadNode_AllNodesHealthy_ReturnsANode) {
    PartitionMap  pm(path_, {ReplicaGroup{kMaster, {kSlave}}});
    HealthTracker ht({kMaster, kSlave});   // both healthy
    RequestManager rm(pm, ht);

    rm.get_write_node("logs");  // triggers assignment
    auto result = rm.get_read_node("logs");

    ASSERT_TRUE(result.has_value());
    // Result must be one of the known nodes.
    bool is_master = result->addr() == kMaster.addr();
    bool is_slave  = result->addr() == kSlave.addr();
    EXPECT_TRUE(is_master || is_slave);
}

TEST_F(RequestManagerTest, GetReadNode_NoHealthyNodes_ReturnsNullopt) {
    // HealthTracker with an empty node list knows nothing → every
    // healthy_subset call returns empty regardless of candidates.
    PartitionMap  pm(path_, {ReplicaGroup{kMaster, {kSlave}}});
    HealthTracker ht({});   // no nodes registered → all unknown → all filtered
    RequestManager rm(pm, ht);

    rm.get_write_node("stream");  // triggers assignment
    auto result = rm.get_read_node("stream");

    EXPECT_FALSE(result.has_value());
}

TEST_F(RequestManagerTest, GetReadNode_OnlyMasterHealthy_ReturnsMaster) {
    // kSlave is not registered in HealthTracker → unknown → filtered out.
    PartitionMap  pm(path_, {ReplicaGroup{kMaster, {kSlave}}});
    HealthTracker ht({kMaster});   // only master is known-healthy
    RequestManager rm(pm, ht);

    rm.get_write_node("metrics");
    auto result = rm.get_read_node("metrics");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->addr(), kMaster.addr());
}

TEST_F(RequestManagerTest, GetReadNode_OnlySlaveHealthy_ReturnsSlave) {
    // kMaster is not registered → unknown → filtered out. Only kSlave is.
    PartitionMap  pm(path_, {ReplicaGroup{kMaster, {kSlave}}});
    HealthTracker ht({kSlave});    // only slave is known-healthy
    RequestManager rm(pm, ht);

    rm.get_write_node("audit");
    auto result = rm.get_read_node("audit");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->addr(), kSlave.addr());
}
