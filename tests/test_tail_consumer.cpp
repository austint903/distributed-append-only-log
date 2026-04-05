#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <filesystem>

#include "../src/server/append_log/append_log.h"

using namespace std::chrono_literals;

class TailConsumerTest : public ::testing::Test {
protected:
    std::string path_;

    void SetUp() override {
        path_ = std::filesystem::temp_directory_path() / "test_tail_consumer.log";
        std::filesystem::remove(path_);
    }

    void TearDown() override {
        std::filesystem::remove(path_);
    }

    static std::span<const uint8_t> to_span(const std::string& s) {
        return {reinterpret_cast<const uint8_t*>(s.data()), s.size()};
    }
};

// Data already exists — wait_for_seq should return true immediately
TEST_F(TailConsumerTest, ReturnsTrueImmediatelyWhenDataExists) {
    AppendLog log(path_.c_str());
    log.append_and_seq(to_span("hello"));  // seq=0

    bool result = log.wait_for_seq(0, 100ms);
    EXPECT_TRUE(result);
}

// wait_for_seq blocks until a producer appends
TEST_F(TailConsumerTest, BlocksUntilProducerAppends) {
    AppendLog log(path_.c_str());

    std::atomic<bool> arrived{false};
    std::thread consumer([&] {
        arrived = log.wait_for_seq(0, 5000ms);
    });

    std::this_thread::sleep_for(50ms);
    log.append_and_seq(to_span("hello"));

    consumer.join();
    EXPECT_TRUE(arrived);
}

// wait_for_seq returns false when no data arrives within timeout
TEST_F(TailConsumerTest, ReturnsFalseOnTimeout) {
    AppendLog log(path_.c_str());

    auto start = std::chrono::steady_clock::now();
    bool result = log.wait_for_seq(0, 100ms);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result);
    // should complete in roughly 100ms, allow 2x margin for scheduler jitter
    EXPECT_LT(elapsed, 200ms);
}

// waits for the correct higher offset, not just any append
TEST_F(TailConsumerTest, WaitsForCorrectHigherOffset) {
    AppendLog log(path_.c_str());
    log.append_and_seq(to_span("a"));  // seq=0
    log.append_and_seq(to_span("b"));  // seq=1
    log.append_and_seq(to_span("c"));  // seq=2

    std::atomic<bool> arrived{false};
    std::thread consumer([&] {
        arrived = log.wait_for_seq(4, 5000ms);
    });

    std::this_thread::sleep_for(50ms);
    log.append_and_seq(to_span("d"));  // seq=3
    log.append_and_seq(to_span("e"));  // seq=4

    consumer.join();
    EXPECT_TRUE(arrived);
}

// notify_all wakes every waiting thread, not just one
TEST_F(TailConsumerTest, MultipleWaitersAllWokenByOneAppend) {
    AppendLog log(path_.c_str());

    constexpr int NUM_WAITERS = 5;
    std::atomic<int> woken{0};
    std::vector<std::thread> consumers;

    for (int i = 0; i < NUM_WAITERS; i++) {
        consumers.emplace_back([&] {
            if (log.wait_for_seq(0, 5000ms)) woken++;
        });
    }

    std::this_thread::sleep_for(50ms);
    log.append_and_seq(to_span("wake everyone"));

    for (auto& t : consumers) t.join();
    EXPECT_EQ(woken, NUM_WAITERS);
}

// full tail loop: consumer tracks all 100 appends in order
TEST_F(TailConsumerTest, ConcurrentProducerAndConsumerProduceCorrectSequence) {
    AppendLog log(path_.c_str());

    constexpr int NUM_RECORDS = 100;
    std::vector<uint64_t> observed;
    observed.reserve(NUM_RECORDS);

    std::thread consumer([&] {
        uint64_t offset = 0;
        while (offset < NUM_RECORDS) {
            bool arrived = log.wait_for_seq(offset, 5000ms);
            ASSERT_TRUE(arrived);
            observed.push_back(offset);
            offset++;
        }
    });

    std::thread producer([&] {
        for (int i = 0; i < NUM_RECORDS; i++) {
            log.append_and_seq(to_span(std::to_string(i)));
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(observed.size(), static_cast<size_t>(NUM_RECORDS));
    for (int i = 0; i < NUM_RECORDS; i++) {
        EXPECT_EQ(observed[i], static_cast<uint64_t>(i));
    }
}
