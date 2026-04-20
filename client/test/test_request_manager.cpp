#include <gtest/gtest.h>
#include <core/request_manager.h>

#include <nova/packet.h>
#include <nova/protocol.h>

#include <thread>
#include <chrono>

using namespace nova::client;
using namespace nova::proto;

TEST(RequestManagerTest, AddAndHandleResponse) {
    RequestManager mgr(5000);

    bool called = false;
    uint32_t received_seq = 0;

    mgr.AddPending(100, [&](const Packet& resp) {
        called = true;
        received_seq = resp.seq;
    });

    EXPECT_EQ(mgr.PendingCount(), 1);

    Packet resp;
    resp.cmd = static_cast<uint16_t>(Cmd::kLoginAck);
    resp.seq = 100;
    resp.body = Serialize(LoginAck{0, "ok", "uid123", "test", ""});

    bool matched = mgr.HandleResponse(resp);
    EXPECT_TRUE(matched);
    EXPECT_TRUE(called);
    EXPECT_EQ(received_seq, 100);
    EXPECT_EQ(mgr.PendingCount(), 0);
}

TEST(RequestManagerTest, UnmatchedSeq) {
    RequestManager mgr;

    mgr.AddPending(200, [](const Packet&) {});

    Packet resp;
    resp.seq = 999;
    bool matched = mgr.HandleResponse(resp);
    EXPECT_FALSE(matched);
    EXPECT_EQ(mgr.PendingCount(), 1);
}

TEST(RequestManagerTest, Timeout) {
    RequestManager mgr(50);  // 50ms 超时

    bool timed_out = false;
    uint32_t timeout_seq = 0;

    mgr.AddPending(300,
        [](const Packet&) {},
        [&](uint32_t seq) {
            timed_out = true;
            timeout_seq = seq;
        }
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr.CheckTimeouts();

    EXPECT_TRUE(timed_out);
    EXPECT_EQ(timeout_seq, 300);
    EXPECT_EQ(mgr.PendingCount(), 0);
}

TEST(RequestManagerTest, Cancel) {
    RequestManager mgr;

    mgr.AddPending(400, [](const Packet&) {});
    EXPECT_EQ(mgr.PendingCount(), 1);

    mgr.Cancel(400);
    EXPECT_EQ(mgr.PendingCount(), 0);
}

TEST(RequestManagerTest, CancelAll) {
    RequestManager mgr;

    mgr.AddPending(1, [](const Packet&) {});
    mgr.AddPending(2, [](const Packet&) {});
    mgr.AddPending(3, [](const Packet&) {});
    EXPECT_EQ(mgr.PendingCount(), 3);

    mgr.CancelAll();
    EXPECT_EQ(mgr.PendingCount(), 0);
}

TEST(RequestManagerTest, MultiplePending) {
    RequestManager mgr;

    int call_count = 0;
    for (uint32_t i = 1; i <= 5; ++i) {
        mgr.AddPending(i, [&](const Packet&) { ++call_count; });
    }
    EXPECT_EQ(mgr.PendingCount(), 5);

    // 仅应答 seq=3
    Packet resp;
    resp.seq = 3;
    mgr.HandleResponse(resp);
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(mgr.PendingCount(), 4);
}
