#include <gtest/gtest.h>
#include <memory>
#include "service/router.h"
#include "model/packet.h"

namespace nova {
namespace {

class MockConnection : public Connection {
public:
    void Send(const Packet& /*pkt*/) override {}
    void Close() override {}
};

TEST(RouterTest, DispatchRegisteredCmd) {
    Router router;
    bool called = false;
    uint16_t received_cmd = 0;

    router.Register(Cmd::kSendMsg, [&](ConnectionPtr /*conn*/, Packet& pkt) {
        called = true;
        received_cmd = pkt.cmd;
    });

    auto conn = std::make_shared<MockConnection>();
    conn->set_user_id(1);  // authenticate for non-Login cmd
    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(Cmd::kSendMsg);
    pkt.seq = 1;

    router.Dispatch(conn, pkt);

    EXPECT_TRUE(called);
    EXPECT_EQ(received_cmd, static_cast<uint16_t>(Cmd::kSendMsg));
}

TEST(RouterTest, DispatchUnknownCmdDoesNotCrash) {
    Router router;

    auto conn = std::make_shared<MockConnection>();
    Packet pkt;
    pkt.cmd = 0xFFFF; // 未注册命令

    // 不应崩溃
    EXPECT_NO_THROW(router.Dispatch(conn, pkt));
}

TEST(RouterTest, MultipleHandlers) {
    Router router;
    int login_count = 0;
    int msg_count = 0;

    router.Register(Cmd::kLogin, [&](ConnectionPtr, Packet&) { ++login_count; });
    router.Register(Cmd::kSendMsg, [&](ConnectionPtr, Packet&) { ++msg_count; });

    auto conn = std::make_shared<MockConnection>();
    conn->set_user_id(1);  // authenticate for non-Login cmd

    Packet login_pkt;
    login_pkt.cmd = static_cast<uint16_t>(Cmd::kLogin);

    Packet msg_pkt;
    msg_pkt.cmd = static_cast<uint16_t>(Cmd::kSendMsg);

    router.Dispatch(conn, login_pkt);
    router.Dispatch(conn, msg_pkt);
    router.Dispatch(conn, msg_pkt);

    EXPECT_EQ(login_count, 1);
    EXPECT_EQ(msg_count, 2);
}

TEST(RouterTest, AuthGuardBlocksUnauthenticated) {
    Router router;
    bool called = false;

    router.Register(Cmd::kSendMsg, [&](ConnectionPtr, Packet&) { called = true; });

    auto conn = std::make_shared<MockConnection>();  // user_id == 0, not authenticated
    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(Cmd::kSendMsg);

    router.Dispatch(conn, pkt);
    EXPECT_FALSE(called);  // should be blocked by auth guard
}

TEST(RouterTest, AuthGuardAllowsLogin) {
    Router router;
    bool called = false;

    router.Register(Cmd::kLogin, [&](ConnectionPtr, Packet&) { called = true; });

    auto conn = std::make_shared<MockConnection>();  // not authenticated
    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(Cmd::kLogin);

    router.Dispatch(conn, pkt);
    EXPECT_TRUE(called);  // Login should pass even when unauthenticated
}

} // namespace
} // namespace nova
