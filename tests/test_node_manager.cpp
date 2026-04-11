#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "../src/server/node_manager/node_manager.h"
#include "../src/server/topic_registry/topic_registry.h"

// NodeManager reads ROLE and MASTER_URL from the environment at construction
// time and never starts any threads unless start() is called.  These tests
// exercise only the role-selection logic, so start() is intentionally omitted.
//
// NOTE: This test binary requires liburing and must run inside Docker.

class NodeManagerTest : public ::testing::Test {
protected:
    std::string data_dir_;

    void SetUp() override {
        data_dir_ = std::string("/tmp/nm_test_") +
                    ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::filesystem::remove_all(data_dir_);
        std::filesystem::create_directories(data_dir_);

        // Start each test with a clean environment.
        unsetenv("ROLE");
        unsetenv("MASTER_URL");
    }

    void TearDown() override {
        unsetenv("ROLE");
        unsetenv("MASTER_URL");
        std::filesystem::remove_all(data_dir_);
    }
};

// ─── role selection ────────────────────────────────────────────────────────

TEST_F(NodeManagerTest, IsMaster_NoRoleEnvVar_DefaultsToMaster) {
    TopicRegistry registry(data_dir_);
    NodeManager nm(registry);

    EXPECT_TRUE(nm.is_master());
}

TEST_F(NodeManagerTest, IsMaster_RoleSetToMaster_ReturnsTrue) {
    setenv("ROLE", "master", 1);

    TopicRegistry registry(data_dir_);
    NodeManager nm(registry);

    EXPECT_TRUE(nm.is_master());
}

TEST_F(NodeManagerTest, IsMaster_RoleSetToSlave_WithMasterUrl_ReturnsFalse) {
    setenv("ROLE", "slave", 1);
    setenv("MASTER_URL", "http://127.0.0.1:8080", 1);

    TopicRegistry registry(data_dir_);
    NodeManager nm(registry);

    EXPECT_FALSE(nm.is_master());
}

TEST_F(NodeManagerTest, IsMaster_RoleSetToSlave_NoMasterUrl_FallsBackToMaster) {
    // When ROLE=slave but MASTER_URL is missing the constructor logs a warning
    // and falls back to master so the node can still function.
    setenv("ROLE", "slave", 1);
    // MASTER_URL intentionally not set.

    TopicRegistry registry(data_dir_);
    NodeManager nm(registry);

    EXPECT_TRUE(nm.is_master());
}

TEST_F(NodeManagerTest, IsMaster_RoleSetToUnrecognisedValue_DefaultsToMaster) {
    setenv("ROLE", "replica", 1);  // not "slave" → treated as master

    TopicRegistry registry(data_dir_);
    NodeManager nm(registry);

    EXPECT_TRUE(nm.is_master());
}

// ─── lifecycle ─────────────────────────────────────────────────────────────

TEST_F(NodeManagerTest, MasterNode_StopWithoutStart_DoesNotCrash) {
    TopicRegistry registry(data_dir_);
    // Destructor calls stop() — must not deadlock or crash.
    EXPECT_NO_THROW({ NodeManager nm(registry); });
}

TEST_F(NodeManagerTest, SlaveNode_StartAndStop_DoesNotCrash) {
    // start() spawns the discovery thread; stop() must join it cleanly.
    // The thread will attempt an HTTP call to an unreachable host and loop,
    // but stop_ is set before join so it terminates promptly.
    setenv("ROLE", "slave", 1);
    setenv("MASTER_URL", "http://127.0.0.1:19999", 1);  // nothing listening

    TopicRegistry registry(data_dir_);
    NodeManager nm(registry);

    EXPECT_NO_THROW({
        nm.start();
        nm.stop();
    });
}
