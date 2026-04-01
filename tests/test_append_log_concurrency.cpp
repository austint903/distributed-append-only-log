#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <unordered_set>
#include <string>
#include <unistd.h>

#include "../src/server/append_log/append_log.h"

class AppendLogConcurrencyTest : public ::testing::Test {
protected:
    std::string path_;
    AppendLog*  log_ = nullptr;

    void SetUp() override {
        path_ = std::string("/tmp/append_log_concurrency_") +
                ::testing::UnitTest::GetInstance()->current_test_info()->name() +
                ".bin";
        ::unlink(path_.c_str());
        log_ = new AppendLog(path_.c_str());
    }

    void TearDown() override {
        delete log_;
        log_ = nullptr;
        ::unlink(path_.c_str());
    }

    static std::vector<uint8_t> make_payload(int thread_id, int msg_id) {
        std::string s = "t" + std::to_string(thread_id) + "-m" + std::to_string(msg_id);
        return {s.begin(), s.end()};
    }
};

TEST_F(AppendLogConcurrencyTest, ConcurrentAppendsProduceUniqueSequenceNumbers) {
    constexpr int THREADS      = 2;
    constexpr int APPENDS_EACH = 50;
    constexpr int TOTAL        = THREADS * APPENDS_EACH;

    std::vector<uint64_t> collected[THREADS];
    std::vector<std::thread> workers;

    for (int t = 0; t < THREADS; ++t) {
        workers.emplace_back([&, t] {
            for (int i = 0; i < APPENDS_EACH; ++i) {
                auto payload = make_payload(t, i);
                uint64_t seq = log_->append_and_seq(payload);
                collected[t].push_back(seq);
            }
        });
    }
    for (auto& w : workers) w.join();

    std::unordered_set<uint64_t> seen;
    for (int t = 0; t < THREADS; ++t) {
        for (uint64_t seq : collected[t]) {
            EXPECT_TRUE(seen.insert(seq).second)
                << "Duplicate sequence number: " << seq;
        }
    }
    EXPECT_EQ(static_cast<int>(seen.size()), TOTAL);
    for (int i = 0; i < TOTAL; ++i)
        EXPECT_TRUE(seen.count(i)) << "Missing sequence number: " << i;
}

TEST_F(AppendLogConcurrencyTest, CommittedSequenceIsVisibleToGetOffset) {
    constexpr int PRELOAD           = 50;
    constexpr int CONCURRENT_WRITES = 50;

    for (int i = 0; i < PRELOAD; ++i)
        log_->append_and_seq(make_payload(0, i));

    std::thread writer([&] {
        for (int i = 0; i < CONCURRENT_WRITES; ++i)
            log_->append_and_seq(make_payload(1, i));
    });

    std::thread reader([&] {
        for (int seq = 0; seq < PRELOAD; ++seq) {
            auto offset = log_->getOffset(static_cast<uint64_t>(seq));
            EXPECT_TRUE(offset.has_value())
                << "getOffset returned nullopt for committed seq " << seq;
        }
    });

    writer.join();
    reader.join();
}


TEST_F(AppendLogConcurrencyTest, UdpAndHttpThreadsBothAppendingProducesCorrectCount) {
    constexpr int UDP_APPENDS  = 75;
    constexpr int HTTP_APPENDS = 75;
    constexpr int TOTAL        = UDP_APPENDS + HTTP_APPENDS;

    std::thread udp_thread([&] {
        for (int i = 0; i < UDP_APPENDS; ++i)
            log_->append_and_seq(make_payload(0, i));
    });

    std::thread http_thread([&] {
        for (int i = 0; i < HTTP_APPENDS; ++i)
            log_->append_and_seq(make_payload(1, i));
    });

    udp_thread.join();
    http_thread.join();

    EXPECT_EQ(log_->next_seq(), static_cast<uint64_t>(TOTAL));
}
