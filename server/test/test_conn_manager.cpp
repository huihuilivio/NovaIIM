#include <gtest/gtest.h>
#include <memory>
#include "net/conn_manager.h"
#include "model/packet.h"

namespace nova {
namespace {

// 测试用的 Mock Connection
class MockConnection : public Connection {
public:
    void Send(const Packet& /*pkt*/) override { ++send_count; }
    void SendEncoded(const std::string& /*data*/) override { ++send_count; }
    void Close() override { closed = true; }

    int  send_count = 0;
    bool closed = false;
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

} // namespace
} // namespace nova
