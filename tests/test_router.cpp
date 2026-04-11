#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

#include "../src/router/partition_map.h"
#include "../src/router/router_server.h"
#include "../vendor/httplib.h"

static const NodeInfo kNodeA{"127.0.0.1", 8080};
static const NodeInfo kNodeB{"127.0.0.1", 8081};
static const NodeInfo kSlave1{"127.0.0.1", 8082};

// ─── NodeInfo ──────────────────────────────────────────────────────────────

TEST(NodeInfo, AddrFormatsAsHostColonPort) {
    NodeInfo n{"localhost", 9090};
    EXPECT_EQ(n.addr(), "localhost:9090");
}

// ─── PartitionMap ──────────────────────────────────────────────────────────

class PartitionMapTest : public ::testing::Test {
protected:
    std::string dir_;
    std::string path_;

    void SetUp() override {
        dir_ = std::string("/tmp/pm_test_") +
               ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
        path_ = dir_ + "/partitions.json";
    }

    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }

    void write_assignments_file(const std::string& topic, const std::string& addr) {
        std::ofstream f(path_);
        f << "{\n  \"assignments\": {\n"
          << "    \"" << topic << "\": \"" << addr << "\"\n"
          << "  }\n}\n";
    }
};

TEST_F(PartitionMapTest, SnapshotIsEmptyWithNoAssignments) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}});

    EXPECT_TRUE(map.snapshot().empty());
}

TEST_F(PartitionMapTest, GetOrAssign_NewTopic_ReturnsAvailableNode) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}});

    auto node = map.get_or_assign("orders");

    EXPECT_EQ(node.host, kNodeA.host);
    EXPECT_EQ(node.port, kNodeA.port);
}

TEST_F(PartitionMapTest, GetOrAssign_ExistingTopic_ReturnsSameNode) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}, ReplicaGroup{kNodeB, {}}});

    auto first  = map.get_or_assign("payments");
    auto second = map.get_or_assign("payments");

    EXPECT_EQ(first.host, second.host);
    EXPECT_EQ(first.port, second.port);
}

TEST_F(PartitionMapTest, GetOrAssign_NewTopicAppearsInSnapshot) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}});

    map.get_or_assign("events");

    auto snap = map.snapshot();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_NE(snap.find("events"), snap.end());
}

TEST_F(PartitionMapTest, GetOrAssign_PersistsAssignmentToFile) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}});

    map.get_or_assign("metrics");

    EXPECT_TRUE(std::filesystem::exists(path_));
}

TEST_F(PartitionMapTest, GetOrAssign_TwoNodes_DistributesTopicsEvenly) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}, ReplicaGroup{kNodeB, {}}});

    map.get_or_assign("t1");
    map.get_or_assign("t2");
    map.get_or_assign("t3");
    map.get_or_assign("t4");

    std::unordered_map<std::string, int> counts;
    for (auto& [topic, addr] : map.snapshot()) counts[addr]++;
    EXPECT_EQ(counts[kNodeA.addr()], 2);
    EXPECT_EQ(counts[kNodeB.addr()], 2);
}

TEST_F(PartitionMapTest, GetOrAssign_PicksLeastLoadedNode) {
    // nodeA already has one topic in the persisted file → nodeB is least loaded
    write_assignments_file("existing", kNodeA.addr());

    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}, ReplicaGroup{kNodeB, {}}});
    map.load();

    auto node = map.get_or_assign("newcomer");

    EXPECT_EQ(node.host, kNodeB.host);
    EXPECT_EQ(node.port, kNodeB.port);
}

TEST_F(PartitionMapTest, Load_MissingFile_DoesNotCrashAndLeavesMapEmpty) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}});  // file does not exist yet

    EXPECT_NO_THROW(map.load());
    EXPECT_TRUE(map.snapshot().empty());
}

TEST_F(PartitionMapTest, Load_ValidFile_RestoresAssignments) {
    write_assignments_file("logs", kNodeA.addr());

    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}});
    map.load();

    auto snap = map.snapshot();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap.at("logs"), kNodeA.addr());
}

TEST_F(PartitionMapTest, PersistAndReload_AssignmentsSurviveRestart) {
    {
        PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}, ReplicaGroup{kNodeB, {}}});
        map.get_or_assign("alpha");
        map.get_or_assign("beta");
    }

    PartitionMap map2(path_, {ReplicaGroup{kNodeA, {}}, ReplicaGroup{kNodeB, {}}});
    map2.load();

    auto snap = map2.snapshot();
    EXPECT_NE(snap.find("alpha"), snap.end());
    EXPECT_NE(snap.find("beta"),  snap.end());
}

TEST_F(PartitionMapTest, GetOrAssign_LoadedAssignmentNotOverriddenByBalance) {
    // "logs" is pinned to nodeA; even though nodeB is less loaded, loaded
    // assignments must not be moved.
    write_assignments_file("logs", kNodeA.addr());

    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}, ReplicaGroup{kNodeB, {}}});
    map.load();

    auto node = map.get_or_assign("logs");

    EXPECT_EQ(node.host, kNodeA.host);
    EXPECT_EQ(node.port, kNodeA.port);
}

// ─── PartitionMap::get_all_nodes ──────────────────────────────────────────

TEST_F(PartitionMapTest, GetAllNodes_UnassignedTopic_ReturnsEmpty) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {kSlave1}}});

    auto nodes = map.get_all_nodes("never-assigned");

    EXPECT_TRUE(nodes.empty());
}

TEST_F(PartitionMapTest, GetAllNodes_AssignedTopic_ReturnsMasterAndSlaves) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {kSlave1}}});
    map.get_or_assign("stream");

    auto nodes = map.get_all_nodes("stream");

    ASSERT_EQ(nodes.size(), 2u);
    EXPECT_EQ(nodes[0].addr(), kNodeA.addr());
    EXPECT_EQ(nodes[1].addr(), kSlave1.addr());
}

TEST_F(PartitionMapTest, GetAllNodes_MasterOnlyGroup_ReturnsMasterOnly) {
    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}});
    map.get_or_assign("solo");

    auto nodes = map.get_all_nodes("solo");

    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].addr(), kNodeA.addr());
}

TEST_F(PartitionMapTest, GetAllNodes_OrphanAssignment_ReturnsEmpty) {
    // Assignment file references a node not in any ReplicaGroup.
    // get_all_nodes cannot look up the group → returns empty rather than crash.
    write_assignments_file("orphan", "10.0.0.99:9999");

    PartitionMap map(path_, {ReplicaGroup{kNodeA, {}}});
    map.load();

    auto nodes = map.get_all_nodes("orphan");

    EXPECT_TRUE(nodes.empty());
}

// ─── RouterServer ──────────────────────────────────────────────────────────
//
// Each test spins up a real RouterServer whose single backend is on port 1
// (always refused), letting us exercise the router's own routing and error
// paths without a live backend.

static int next_port() {
    static std::atomic<int> counter{19100};
    return counter.fetch_add(1);
}

class RouterServerTest : public ::testing::Test {
protected:
    std::string dir_;
    std::string path_;
    int port_{};
    std::unique_ptr<PartitionMap> pm_;
    std::unique_ptr<RouterServer> router_;
    std::thread server_thread_;

    void SetUp() override {
        dir_ = std::string("/tmp/router_test_") +
               ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
        path_ = dir_ + "/partitions.json";
        port_ = next_port();

        pm_ = std::make_unique<PartitionMap>(path_, std::vector<ReplicaGroup>{ReplicaGroup{{"127.0.0.1", 1}, {}}});
        router_ = std::make_unique<RouterServer>(*pm_, std::vector<NodeInfo>{{"127.0.0.1", 1}}, port_);

        server_thread_ = std::thread([this] { router_->start(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        router_->stop();
        server_thread_.join();
        std::filesystem::remove_all(dir_);
    }

    httplib::Client client() {
        httplib::Client cli("127.0.0.1", port_);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(2, 0);
        return cli;
    }
};

TEST_F(RouterServerTest, Produce_MissingTopicParam_Returns400) {
    auto cli = client();
    auto res = cli.Post("/produce", "", "text/plain");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(RouterServerTest, Consume_MissingTopicParam_Returns400) {
    auto cli = client();
    auto res = cli.Get("/consume");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(RouterServerTest, Tail_MissingTopicParam_Returns400) {
    auto cli = client();
    auto res = cli.Get("/tail");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(RouterServerTest, Produce_BackendUnreachable_Returns502) {
    auto cli = client();
    auto res = cli.Post("/produce?topic=orders", "hello", "text/plain");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 502);
}

TEST_F(RouterServerTest, Consume_BackendUnreachable_Returns502) {
    auto cli = client();
    auto res = cli.Get("/consume?topic=orders");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 502);
}

TEST_F(RouterServerTest, Tail_BackendUnreachable_Returns502) {
    auto cli = client();
    auto res = cli.Get("/tail?topic=orders&timeout_ms=100");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 502);
}

TEST_F(RouterServerTest, Topics_EmptyMap_ReturnsEmptyTopicsList) {
    auto cli = client();
    auto res = cli.Get("/topics");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->body, "{\"topics\":[]}");
}

TEST_F(RouterServerTest, Topics_AfterAssignment_IncludesAssignedTopic) {
    pm_->get_or_assign("events");

    auto cli = client();
    auto res = cli.Get("/topics");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("\"events\""), std::string::npos);
}
