#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../src/server/topic_registry/topic_registry.h"

class TopicRegistryTest : public ::testing::Test {
protected:
    std::string dir_;

    void SetUp() override {
        dir_ = std::string("/tmp/topic_registry_") +
               ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }

    static std::vector<uint8_t> to_bytes(const std::string& s) {
        return {s.begin(), s.end()};
    }
};

// --- list_topics ---

TEST_F(TopicRegistryTest, EmptyDataDirHasNoTopics) {
    TopicRegistry registry(dir_);

    EXPECT_TRUE(registry.list_topics().empty());
}

TEST_F(TopicRegistryTest, ListTopicsIncludesCreatedTopic) {
    TopicRegistry registry(dir_);
    registry.get_or_create("metrics");

    auto topics = registry.list_topics();
    ASSERT_EQ(topics.size(), 1u);
    EXPECT_EQ(topics[0], "metrics");
}

TEST_F(TopicRegistryTest, ListTopicsReturnsSortedOrder) {
    TopicRegistry registry(dir_);
    registry.get_or_create("zebra");
    registry.get_or_create("alpha");
    registry.get_or_create("mango");

    auto topics = registry.list_topics();
    ASSERT_EQ(topics.size(), 3u);
    EXPECT_EQ(topics[0], "alpha");
    EXPECT_EQ(topics[1], "mango");
    EXPECT_EQ(topics[2], "zebra");
}

// --- get_or_create ---

TEST_F(TopicRegistryTest, GetOrCreateNewTopicCreatesFileOnDisk) {
    TopicRegistry registry(dir_);
    registry.get_or_create("metrics");

    EXPECT_TRUE(std::filesystem::exists(dir_ + "/metrics_log.log"));
}

TEST_F(TopicRegistryTest, GetOrCreateReturnsSameEntryOnSubsequentCalls) {
    TopicRegistry registry(dir_);

    auto& first  = registry.get_or_create("metrics");
    auto& second = registry.get_or_create("metrics");

    EXPECT_EQ(first.log.get(), second.log.get());
}

TEST_F(TopicRegistryTest, GetOrCreateDoesNotCreateDuplicateFiles) {
    TopicRegistry registry(dir_);
    registry.get_or_create("metrics");
    registry.get_or_create("metrics");

    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir_)) {
        if (entry.path().filename() == "metrics_log.log") count++;
    }
    EXPECT_EQ(count, 1);
}

// --- append + read ---

TEST_F(TopicRegistryTest, AppendedPayloadIsReadBackFromTopic) {
    TopicRegistry registry(dir_);
    auto& entry = registry.get_or_create("orders");
    const auto payload = to_bytes("hello");

    entry.log->append(payload);
    auto result = entry.reader->read(0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

TEST_F(TopicRegistryTest, TwoTopicsAreIndependent) {
    TopicRegistry registry(dir_);
    auto& a = registry.get_or_create("a");
    auto& b = registry.get_or_create("b");

    a.log->append(to_bytes("only in a"));

    EXPECT_FALSE(b.reader->read(0).has_value());
}

// --- recovery ---

TEST_F(TopicRegistryTest, ConstructorRecoversPreviouslyWrittenTopic) {
    {
        TopicRegistry registry(dir_);
        registry.get_or_create("orders");
    }

    TopicRegistry registry(dir_);
    auto topics = registry.list_topics();
    ASSERT_EQ(topics.size(), 1u);
    EXPECT_EQ(topics[0], "orders");
}

TEST_F(TopicRegistryTest, RecoveredTopicPreservesWrittenData) {
    const auto payload = to_bytes("important record");
    {
        TopicRegistry registry(dir_);
        registry.get_or_create("events").log->append(payload);
    }

    TopicRegistry registry(dir_);
    auto result = registry.get_or_create("events").reader->read(0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

TEST_F(TopicRegistryTest, RecoveredTopicNextSeqContinuesFromLastRecord) {
    {
        TopicRegistry registry(dir_);
        auto& entry = registry.get_or_create("events");
        entry.log->append(to_bytes("first"));
        entry.log->append(to_bytes("second"));
    }

    TopicRegistry registry(dir_);
    auto& entry = registry.get_or_create("events");
    uint64_t seq = entry.log->append_and_seq(to_bytes("third"));

    EXPECT_EQ(seq, 2u);
}

// --- constructor scanning ---

TEST_F(TopicRegistryTest, ConstructorIgnoresNonMatchingFiles) {
    std::ofstream(dir_ + "/readme.txt").close();
    std::ofstream(dir_ + "/data.bin").close();

    TopicRegistry registry(dir_);

    EXPECT_TRUE(registry.list_topics().empty());
}
