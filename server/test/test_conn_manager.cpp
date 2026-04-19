#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include "net/conn_manager.h"
#include <nova/packet.h>

namespace nova {
namespace {

// 测试用的 Mock Connection
class MockConnection : public Connection {
public:
    void Send(const Packet& /*pkt*/) override { ++send_count; }
    void SendEncoded(const std::string& /*data*/) override { ++send_count; }
    void Close() override { closed = true; }

    int send_count = 0;
    bool closed    = false;
};

class ConnManagerTest : public ::testing::Test {
protected:
    ConnManager mgr_;  // 每个测试用独立实例，天然隔离
};

TEST_F(ConnManagerTest, AddAndGetConns) {
    auto conn = std::make_shared<MockConnection>();
    conn->set_user_id(1001);
    conn->set_device_id("pc-001");

    mgr_.Add(1001, conn);

    auto conns = mgr_.GetConns(1001);
    ASSERT_EQ(conns.size(), 1u);
    EXPECT_EQ(conns[0]->device_id(), "pc-001");

    // cleanup
    mgr_.Remove(1001, conn.get());
}

TEST_F(ConnManagerTest, IsOnline) {
    EXPECT_FALSE(mgr_.IsOnline(9999));

    auto conn = std::make_shared<MockConnection>();
    conn->set_user_id(9999);
    mgr_.Add(9999, conn);

    EXPECT_TRUE(mgr_.IsOnline(9999));

    mgr_.Remove(9999, conn.get());
    EXPECT_FALSE(mgr_.IsOnline(9999));
}

TEST_F(ConnManagerTest, MultiDevice) {
    auto pc = std::make_shared<MockConnection>();
    pc->set_user_id(2001);
    pc->set_device_id("pc");

    auto mobile = std::make_shared<MockConnection>();
    mobile->set_user_id(2001);
    mobile->set_device_id("mobile");

    mgr_.Add(2001, pc);
    mgr_.Add(2001, mobile);

    auto conns = mgr_.GetConns(2001);
    EXPECT_EQ(conns.size(), 2u);

    auto found = mgr_.GetConn(2001, "mobile");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->device_id(), "mobile");

    // cleanup
    mgr_.Remove(2001, pc.get());
    mgr_.Remove(2001, mobile.get());
}

TEST_F(ConnManagerTest, RemoveNonExistent) {
    auto conn = std::make_shared<MockConnection>();
    conn->set_user_id(3001);

    // 不应崩溃
    mgr_.Remove(3001, conn.get());
    EXPECT_FALSE(mgr_.IsOnline(3001));
}

TEST_F(ConnManagerTest, OnlineCountAccurate) {
    EXPECT_EQ(mgr_.online_count(), 0);

    auto c1 = std::make_shared<MockConnection>();
    c1->set_user_id(1);
    mgr_.Add(1, c1);
    EXPECT_EQ(mgr_.online_count(), 1);

    auto c2 = std::make_shared<MockConnection>();
    c2->set_user_id(2);
    mgr_.Add(2, c2);
    EXPECT_EQ(mgr_.online_count(), 2);

    // 同用户多设备不增加计数
    auto c1b = std::make_shared<MockConnection>();
    c1b->set_user_id(1);
    c1b->set_device_id("mobile");
    mgr_.Add(1, c1b);
    EXPECT_EQ(mgr_.online_count(), 2);

    mgr_.Remove(1, c1.get());
    EXPECT_EQ(mgr_.online_count(), 2);  // user 1 still has c1b

    mgr_.Remove(1, c1b.get());
    EXPECT_EQ(mgr_.online_count(), 1);  // user 1 fully offline

    mgr_.Remove(2, c2.get());
    EXPECT_EQ(mgr_.online_count(), 0);
}

TEST_F(ConnManagerTest, GetConnByDeviceNotFound) {
    auto conn = std::make_shared<MockConnection>();
    conn->set_user_id(4001);
    conn->set_device_id("pc");
    mgr_.Add(4001, conn);

    // 查找不存在的设备
    EXPECT_EQ(mgr_.GetConn(4001, "tablet"), nullptr);

    // 查找不存在的用户
    EXPECT_EQ(mgr_.GetConn(9999, "pc"), nullptr);

    mgr_.Remove(4001, conn.get());
}

TEST_F(ConnManagerTest, ConcurrentAddRemove) {
    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 100;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < kOpsPerThread; ++i) {
                int64_t user_id = t * 1000 + i;
                auto conn = std::make_shared<MockConnection>();
                conn->set_user_id(user_id);
                conn->set_device_id("dev");

                mgr_.Add(user_id, conn);
                EXPECT_TRUE(mgr_.IsOnline(user_id));
                mgr_.Remove(user_id, conn.get());
            }
        });
    }

    for (auto& t : threads)
        t.join();

    // All users removed
    EXPECT_EQ(mgr_.online_count(), 0);
}

}  // namespace
}  // namespace nova
