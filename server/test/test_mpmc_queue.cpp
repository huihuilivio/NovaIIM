#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include "core/mpmc_queue.h"

namespace nova {
namespace {

TEST(MPMCQueueTest, BasicPushPop) {
    MPMCQueue<int> q(16);

    EXPECT_TRUE(q.Push(42));

    int val = 0;
    EXPECT_TRUE(q.Pop(val));
    EXPECT_EQ(val, 42);
}

TEST(MPMCQueueTest, EmptyPopFails) {
    MPMCQueue<int> q(16);

    int val = 0;
    EXPECT_FALSE(q.Pop(val));
}

TEST(MPMCQueueTest, FullPushFails) {
    MPMCQueue<int> q(4);  // capacity = 4

    EXPECT_TRUE(q.Push(1));
    EXPECT_TRUE(q.Push(2));
    EXPECT_TRUE(q.Push(3));
    EXPECT_TRUE(q.Push(4));
    EXPECT_FALSE(q.Push(5));  // 满了
}

TEST(MPMCQueueTest, FIFO) {
    MPMCQueue<int> q(8);

    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(q.Push(i));
    }

    for (int i = 0; i < 8; ++i) {
        int val = -1;
        EXPECT_TRUE(q.Pop(val));
        EXPECT_EQ(val, i);
    }
}

TEST(MPMCQueueTest, ConcurrentProducerConsumer) {
    constexpr int kCapacity  = 1024;
    constexpr int kItemCount = 10000;
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;

    MPMCQueue<int> q(kCapacity);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<int64_t> sum_produced{0};
    std::atomic<int64_t> sum_consumed{0};

    // Producers
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&] {
            int local = 0;
            while (local < kItemCount) {
                int val = produced.fetch_add(1, std::memory_order_relaxed);
                if (val >= kProducers * kItemCount) {
                    produced.fetch_sub(1, std::memory_order_relaxed);
                    break;
                }
                while (!q.Push(val)) {
                    std::this_thread::yield();
                }
                sum_produced.fetch_add(val, std::memory_order_relaxed);
                ++local;
            }
        });
    }

    // Consumers
    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            int val;
            while (consumed.load(std::memory_order_relaxed) < kProducers * kItemCount) {
                if (q.Pop(val)) {
                    sum_consumed.fetch_add(val, std::memory_order_relaxed);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers)
        t.join();
    for (auto& t : consumers)
        t.join();

    EXPECT_EQ(consumed.load(), kProducers * kItemCount);
    EXPECT_EQ(sum_produced.load(), sum_consumed.load());
}

}  // namespace
}  // namespace nova
