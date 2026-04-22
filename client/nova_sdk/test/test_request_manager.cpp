// test_request_manager.cpp — RequestManager 单元测试
//
// 覆盖: AddPending / HandleResponse / CheckTimeouts / Cancel / CancelAll / PendingCount

#include <core/request_manager.h>
#include <nova/packet.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using nova::client::RequestManager;
using nova::proto::Packet;

TEST(RequestManagerTest, InitialPendingCountIsZero) {
    RequestManager mgr;
    EXPECT_EQ(mgr.PendingCount(), 0u);
}

TEST(RequestManagerTest, AddPendingIncrementsCount) {
    RequestManager mgr;
    mgr.AddPending(1, [](const Packet&) {});
    mgr.AddPending(2, [](const Packet&) {});
    EXPECT_EQ(mgr.PendingCount(), 2u);
}

TEST(RequestManagerTest, HandleResponseTriggersCallbackAndRemoves) {
    RequestManager mgr;
    std::atomic<int> hits{0};
    mgr.AddPending(42, [&hits](const Packet& p) {
        EXPECT_EQ(p.seq, 42u);
        hits.fetch_add(1);
    });

    Packet resp;
    resp.seq = 42;
    EXPECT_TRUE(mgr.HandleResponse(resp));
    EXPECT_EQ(hits.load(), 1);
    EXPECT_EQ(mgr.PendingCount(), 0u);
}

TEST(RequestManagerTest, HandleResponseUnknownSeqReturnsFalse) {
    RequestManager mgr;
    Packet resp;
    resp.seq = 99;
    EXPECT_FALSE(mgr.HandleResponse(resp));
}

TEST(RequestManagerTest, HandleResponseDoesNotInvokeOtherCallbacks) {
    RequestManager mgr;
    std::atomic<int> wrong_hits{0};
    mgr.AddPending(1, [&wrong_hits](const Packet&) { wrong_hits.fetch_add(1); });

    Packet resp;
    resp.seq = 2;
    EXPECT_FALSE(mgr.HandleResponse(resp));
    EXPECT_EQ(wrong_hits.load(), 0);
    EXPECT_EQ(mgr.PendingCount(), 1u);
}

TEST(RequestManagerTest, CancelRemovesPending) {
    RequestManager mgr;
    mgr.AddPending(1, [](const Packet&) {});
    mgr.AddPending(2, [](const Packet&) {});
    mgr.Cancel(1);
    EXPECT_EQ(mgr.PendingCount(), 1u);

    Packet resp;
    resp.seq = 1;
    EXPECT_FALSE(mgr.HandleResponse(resp));
}

TEST(RequestManagerTest, CancelAllClearsAll) {
    RequestManager mgr;
    mgr.AddPending(1, [](const Packet&) {});
    mgr.AddPending(2, [](const Packet&) {});
    mgr.AddPending(3, [](const Packet&) {});
    mgr.CancelAll();
    EXPECT_EQ(mgr.PendingCount(), 0u);
}

TEST(RequestManagerTest, CheckTimeoutsTriggersTimeoutCallback) {
    RequestManager mgr(/*default_timeout_ms=*/10);
    std::atomic<uint32_t> timed_out_seq{0};
    mgr.AddPending(7,
        [](const Packet&) { FAIL() << "response should not fire"; },
        [&timed_out_seq](uint32_t seq) { timed_out_seq.store(seq); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mgr.CheckTimeouts();
    EXPECT_EQ(timed_out_seq.load(), 7u);
    EXPECT_EQ(mgr.PendingCount(), 0u);
}

TEST(RequestManagerTest, CheckTimeoutsLeavesAliveRequests) {
    RequestManager mgr(/*default_timeout_ms=*/60000);
    mgr.AddPending(1, [](const Packet&) {});
    mgr.CheckTimeouts();
    EXPECT_EQ(mgr.PendingCount(), 1u);
}

TEST(RequestManagerTest, ResponseAfterTimeoutDoesNotFire) {
    RequestManager mgr(/*default_timeout_ms=*/10);
    std::atomic<int> resp_hits{0};
    mgr.AddPending(5, [&resp_hits](const Packet&) { resp_hits.fetch_add(1); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mgr.CheckTimeouts();

    Packet resp;
    resp.seq = 5;
    EXPECT_FALSE(mgr.HandleResponse(resp));
    EXPECT_EQ(resp_hits.load(), 0);
}

TEST(RequestManagerTest, AddPendingOverwritesSameSeq) {
    RequestManager mgr;
    std::atomic<int> first_hits{0};
    std::atomic<int> second_hits{0};
    mgr.AddPending(1, [&first_hits](const Packet&) { first_hits.fetch_add(1); });
    mgr.AddPending(1, [&second_hits](const Packet&) { second_hits.fetch_add(1); });
    EXPECT_EQ(mgr.PendingCount(), 1u);

    Packet resp;
    resp.seq = 1;
    EXPECT_TRUE(mgr.HandleResponse(resp));
    EXPECT_EQ(first_hits.load(), 0);
    EXPECT_EQ(second_hits.load(), 1);
}

TEST(RequestManagerTest, CallbackWithoutTimeoutIsSafe) {
    RequestManager mgr(/*default_timeout_ms=*/10);
    mgr.AddPending(1, [](const Packet&) {});  // 无超时回调

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_NO_THROW(mgr.CheckTimeouts());
    EXPECT_EQ(mgr.PendingCount(), 0u);
}
