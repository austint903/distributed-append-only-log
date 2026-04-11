#include <gtest/gtest.h>
#include "../src/router/health_tracker.h"
#include "../src/router/partition_map.h"

// HealthTracker initialises every constructor node to healthy=true in health_.
// The poll thread is never started in these tests, so health state is
// deterministic and controlled entirely by the constructor argument list.

static const NodeInfo kA{"10.0.0.1", 8080};
static const NodeInfo kB{"10.0.0.2", 8080};
static const NodeInfo kC{"10.0.0.3", 8080};

// ─── is_healthy ────────────────────────────────────────────────────────────

TEST(HealthTracker, IsHealthy_ConstructorNode_StartsHealthy) {
    HealthTracker ht({kA, kB});

    EXPECT_TRUE(ht.is_healthy(kA.addr()));
    EXPECT_TRUE(ht.is_healthy(kB.addr()));
}

TEST(HealthTracker, IsHealthy_UnknownAddr_ReturnsFalse) {
    HealthTracker ht({kA});

    EXPECT_FALSE(ht.is_healthy(kC.addr()));
}

TEST(HealthTracker, IsHealthy_EmptyNodeList_ReturnsFalseForAnyAddr) {
    HealthTracker ht({});

    EXPECT_FALSE(ht.is_healthy(kA.addr()));
}

// ─── healthy_subset ────────────────────────────────────────────────────────

TEST(HealthTracker, HealthySubset_EmptyCandidateList_ReturnsEmpty) {
    HealthTracker ht({kA, kB});

    auto result = ht.healthy_subset({});

    EXPECT_TRUE(result.empty());
}

TEST(HealthTracker, HealthySubset_AllCandidatesKnownHealthy_ReturnsAll) {
    HealthTracker ht({kA, kB});

    auto result = ht.healthy_subset({kA, kB});

    ASSERT_EQ(result.size(), 2u);
}

TEST(HealthTracker, HealthySubset_NoCandidatesKnown_ReturnsEmpty) {
    // kC was never registered so it is not in health_ at all.
    HealthTracker ht({kA, kB});

    auto result = ht.healthy_subset({kC});

    EXPECT_TRUE(result.empty());
}

TEST(HealthTracker, HealthySubset_MixedKnownUnknown_ReturnsOnlyKnown) {
    HealthTracker ht({kA});

    auto result = ht.healthy_subset({kA, kC});

    // kA is known-healthy; kC is unknown → filtered out.
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].addr(), kA.addr());
}

TEST(HealthTracker, HealthySubset_SingleKnownHealthyNode_ReturnsThatNode) {
    HealthTracker ht({kA});

    auto result = ht.healthy_subset({kA});

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].addr(), kA.addr());
}

// ─── lifecycle ─────────────────────────────────────────────────────────────

TEST(HealthTracker, StopWithoutStart_DoesNotCrash) {
    HealthTracker ht({kA});
    // stop() called implicitly by destructor — must not crash or deadlock.
    EXPECT_NO_THROW({ HealthTracker tmp({kA}); });
}

TEST(HealthTracker, StartThenStop_DoesNotCrash) {
    HealthTracker ht({});  // empty node list: poll loop exits immediately on stop
    EXPECT_NO_THROW({
        ht.start();
        ht.stop();
    });
}
