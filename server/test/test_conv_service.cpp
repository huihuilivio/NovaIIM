// test_conv_service.cpp — ConvService 单元测试
// 使用内存 SQLite + MockConnection 测试会话管理完整链路

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "core/app_config.h"
#include "core/server_context.h"
#include "dao/dao_factory.h"
#include "dao/conversation_dao.h"
#include <nova/packet.h>
#include <nova/protocol.h>
#include <nova/errors.h>
#include "net/connection.h"
#include "service/user_service.h"
#include "service/msg_service.h"
#include "service/friend_service.h"
#include "service/conv_service.h"

namespace nova {
namespace {

// ---- MockConnection ----
class MockConnection : public Connection {
public:
    void Send(const Packet& pkt) override {
        last_pkt = pkt;
        ++send_count;
    }
    void SendEncoded(const std::string& data) override {
        Packet decoded;
        if (Packet::Decode(data.data(), data.size(), decoded)) {
            last_push_pkt = decoded;
        }
        ++push_count;
    }
    void Close() override { closed = true; }

    Packet last_pkt;
    Packet last_push_pkt;
    int send_count = 0;
    int push_count = 0;
    bool closed    = false;
};

template <typename T>
Packet MakePacket(Cmd cmd, const T& body, uint32_t seq = 1, uint64_t uid = 0) {
    Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(cmd);
    pkt.seq  = seq;
    pkt.uid  = uid;
    pkt.body = proto::Serialize(body);
    return pkt;
}

// ---- Test fixture ----
class ConvServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        DatabaseConfig db_cfg;
        db_cfg.type = "sqlite";
        db_cfg.path = ":memory:";

        cfg_.server.recall_timeout_secs = 120;

        ctx_ = std::make_unique<ServerContext>(cfg_);
        ctx_->set_dao(CreateDaoFactory(db_cfg));
        user_svc_   = std::make_unique<UserService>(*ctx_);
        msg_svc_    = std::make_unique<MsgService>(*ctx_);
        friend_svc_ = std::make_unique<FriendService>(*ctx_);
        conv_svc_   = std::make_unique<ConvService>(*ctx_);
    }

    struct UserInfo {
        std::shared_ptr<MockConnection> conn;
        std::string uid;
        int64_t user_id = 0;
    };

    UserInfo CreateAndLogin(const std::string& email, const std::string& nickname,
                            const std::string& device = "dev1") {
        auto reg_conn = std::make_shared<MockConnection>();
        auto reg_pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{email, nickname, "password123"});
        user_svc_->HandleRegister(reg_conn, reg_pkt);
        auto reg_ack = proto::Deserialize<proto::RegisterAck>(reg_conn->last_pkt.body);
        EXPECT_TRUE(reg_ack.has_value());
        EXPECT_EQ(reg_ack->code, 0);

        auto conn      = std::make_shared<MockConnection>();
        auto login_pkt = MakePacket(Cmd::kLogin, proto::LoginReq{email, "password123", device, "pc"});
        user_svc_->HandleLogin(conn, login_pkt);
        auto login_ack = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
        EXPECT_TRUE(login_ack.has_value());
        EXPECT_EQ(login_ack->code, 0);

        return {conn, login_ack->uid, conn->user_id()};
    }

    int64_t MakeFriends(UserInfo& a, UserInfo& b) {
        auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{b.uid, ""});
        friend_svc_->HandleAddFriend(a.conn, add_pkt);
        auto add_ack = proto::Deserialize<proto::AddFriendAck>(a.conn->last_pkt.body);
        EXPECT_EQ(add_ack->code, 0);

        auto h = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{add_ack->request_id, 1});
        friend_svc_->HandleRequest(b.conn, h);
        auto h_ack = proto::Deserialize<proto::HandleFriendReqAck>(b.conn->last_pkt.body);
        EXPECT_EQ(h_ack->code, 0);
        return h_ack->conversation_id;
    }

    void SendMsg(UserInfo& sender, int64_t conv_id, const std::string& content) {
        auto pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, content, proto::MsgType::kText, ""});
        msg_svc_->HandleSendMsg(sender.conn, pkt);
        auto ack = proto::Deserialize<proto::SendMsgAck>(sender.conn->last_pkt.body);
        EXPECT_EQ(ack->code, 0);
    }

    AppConfig cfg_;
    std::unique_ptr<ServerContext> ctx_;
    std::unique_ptr<UserService> user_svc_;
    std::unique_ptr<MsgService> msg_svc_;
    std::unique_ptr<FriendService> friend_svc_;
    std::unique_ptr<ConvService> conv_svc_;
};

// ================================================================
//  GetConvList
// ================================================================

TEST_F(ConvServiceTest, GetConvListEmpty) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto pkt   = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_TRUE(ack->conversations.empty());
}

TEST_F(ConvServiceTest, GetConvListWithConversation) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    ASSERT_EQ(ack->conversations.size(), 1);
    EXPECT_EQ(ack->conversations[0].conversation_id, conv_id);
    EXPECT_EQ(ack->conversations[0].type, static_cast<int>(proto::ConvType::kPrivate));
    // 私聊名称应为对方昵称
    EXPECT_EQ(ack->conversations[0].name, "Bob");
}

TEST_F(ConvServiceTest, GetConvListUnreadCount) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // Alice 发 2 条消息
    SendMsg(alice, conv_id, "hi");
    SendMsg(alice, conv_id, "hello");

    // Bob 查会话列表，应有 2 条未读
    auto pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::GetConvListAck>(bob.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack->conversations.size(), 1);
    EXPECT_EQ(ack->conversations[0].unread_count, 2);
}

TEST_F(ConvServiceTest, GetConvListLastMsg) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    SendMsg(alice, conv_id, "first");
    SendMsg(alice, conv_id, "second");

    auto pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack->conversations.size(), 1);
    EXPECT_EQ(ack->conversations[0].last_msg.content, "second");
}

TEST_F(ConvServiceTest, GetConvListNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(conn, pkt);
    auto ack = proto::Deserialize<proto::GetConvListAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, errc::kNotAuthenticated.code);
}

// ================================================================
//  DeleteConv (隐藏)
// ================================================================

TEST_F(ConvServiceTest, DeleteConvSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // 隐藏会话
    auto pkt = MakePacket(Cmd::kDeleteConv, proto::DeleteConvReq{conv_id});
    conv_svc_->HandleDeleteConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::DeleteConvAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);

    // 确认会话列表中不再可见
    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(list_ack.has_value());
    EXPECT_TRUE(list_ack->conversations.empty());
}

TEST_F(ConvServiceTest, DeleteConvAutoRestore) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // Alice 隐藏会话
    auto del_pkt = MakePacket(Cmd::kDeleteConv, proto::DeleteConvReq{conv_id});
    conv_svc_->HandleDeleteConv(alice.conn, del_pkt);

    // 确认不可见
    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    EXPECT_TRUE(list_ack->conversations.empty());

    // Bob 发消息 → 自动恢复
    SendMsg(bob, conv_id, "are you there?");

    // Alice 重新查询，会话应自动恢复可见
    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(list_ack.has_value());
    ASSERT_EQ(list_ack->conversations.size(), 1);
    EXPECT_EQ(list_ack->conversations[0].conversation_id, conv_id);
}

TEST_F(ConvServiceTest, DeleteConvNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto pkt   = MakePacket(Cmd::kDeleteConv, proto::DeleteConvReq{99999});
    conv_svc_->HandleDeleteConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::DeleteConvAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, errc::conv::kNotMember.code);
}

TEST_F(ConvServiceTest, DeleteConvNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kDeleteConv, proto::DeleteConvReq{1});
    conv_svc_->HandleDeleteConv(conn, pkt);
    auto ack = proto::Deserialize<proto::DeleteConvAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, errc::kNotAuthenticated.code);
}

TEST_F(ConvServiceTest, DeleteConvInvalidBody) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto pkt   = MakePacket(Cmd::kDeleteConv, proto::DeleteConvReq{0});
    conv_svc_->HandleDeleteConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::DeleteConvAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, errc::kInvalidBody.code);
}

// ================================================================
//  MuteConv
// ================================================================

TEST_F(ConvServiceTest, MuteConvSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // 开启免打扰
    auto pkt = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{conv_id, 1});
    conv_svc_->HandleMuteConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::MuteConvAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);

    // 确认会话列表中 mute=1
    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(list_ack->conversations.size(), 1);
    EXPECT_EQ(list_ack->conversations[0].mute, 1);
}

TEST_F(ConvServiceTest, UnmuteConvSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // 开启免打扰
    auto mute_pkt = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{conv_id, 1});
    conv_svc_->HandleMuteConv(alice.conn, mute_pkt);

    // 取消免打扰
    auto unmute_pkt = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{conv_id, 0});
    conv_svc_->HandleMuteConv(alice.conn, unmute_pkt);
    auto ack = proto::Deserialize<proto::MuteConvAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);

    // 确认 mute=0
    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(list_ack->conversations.size(), 1);
    EXPECT_EQ(list_ack->conversations[0].mute, 0);
}

TEST_F(ConvServiceTest, MuteConvNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto pkt   = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{99999, 1});
    conv_svc_->HandleMuteConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::MuteConvAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, errc::conv::kNotMember.code);
}

TEST_F(ConvServiceTest, MuteConvNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{1, 1});
    conv_svc_->HandleMuteConv(conn, pkt);
    auto ack = proto::Deserialize<proto::MuteConvAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, errc::kNotAuthenticated.code);
}

TEST_F(ConvServiceTest, MuteConvInvalidValue) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{conv_id, 5});
    conv_svc_->HandleMuteConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::MuteConvAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::kInvalidBody.code);
}

// ================================================================
//  PinConv
// ================================================================

TEST_F(ConvServiceTest, PinConvSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // 置顶
    auto pkt = MakePacket(Cmd::kPinConv, proto::PinConvReq{conv_id, 1});
    conv_svc_->HandlePinConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::PinConvAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);

    // 确认 pinned=1
    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(list_ack->conversations.size(), 1);
    EXPECT_EQ(list_ack->conversations[0].pinned, 1);
}

TEST_F(ConvServiceTest, UnpinConvSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // 置顶 → 取消置顶
    auto pin_pkt = MakePacket(Cmd::kPinConv, proto::PinConvReq{conv_id, 1});
    conv_svc_->HandlePinConv(alice.conn, pin_pkt);
    auto unpin_pkt = MakePacket(Cmd::kPinConv, proto::PinConvReq{conv_id, 0});
    conv_svc_->HandlePinConv(alice.conn, unpin_pkt);
    auto ack = proto::Deserialize<proto::PinConvAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);

    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(list_ack->conversations.size(), 1);
    EXPECT_EQ(list_ack->conversations[0].pinned, 0);
}

TEST_F(ConvServiceTest, PinConvNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto pkt   = MakePacket(Cmd::kPinConv, proto::PinConvReq{99999, 1});
    conv_svc_->HandlePinConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::PinConvAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, errc::conv::kNotMember.code);
}

TEST_F(ConvServiceTest, PinConvNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kPinConv, proto::PinConvReq{1, 1});
    conv_svc_->HandlePinConv(conn, pkt);
    auto ack = proto::Deserialize<proto::PinConvAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, errc::kNotAuthenticated.code);
}

TEST_F(ConvServiceTest, PinConvInvalidValue) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kPinConv, proto::PinConvReq{conv_id, 3});
    conv_svc_->HandlePinConv(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::PinConvAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::kInvalidBody.code);
}

// ================================================================
//  MuteAndPin 组合
// ================================================================

TEST_F(ConvServiceTest, MuteAndPinIndependent) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // 同时设置 mute 和 pin
    auto mute_pkt = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{conv_id, 1});
    conv_svc_->HandleMuteConv(alice.conn, mute_pkt);
    auto pin_pkt = MakePacket(Cmd::kPinConv, proto::PinConvReq{conv_id, 1});
    conv_svc_->HandlePinConv(alice.conn, pin_pkt);

    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});
    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(list_ack->conversations.size(), 1);
    EXPECT_EQ(list_ack->conversations[0].mute, 1);
    EXPECT_EQ(list_ack->conversations[0].pinned, 1);

    // 取消 mute，pin 不受影响
    auto unmute_pkt = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{conv_id, 0});
    conv_svc_->HandleMuteConv(alice.conn, unmute_pkt);

    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    list_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(list_ack->conversations.size(), 1);
    EXPECT_EQ(list_ack->conversations[0].mute, 0);
    EXPECT_EQ(list_ack->conversations[0].pinned, 1);
}

// ================================================================
//  Per-user 隔离
// ================================================================

TEST_F(ConvServiceTest, DeleteConvPerUser) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // Alice 隐藏，Bob 不受影响
    auto del_pkt = MakePacket(Cmd::kDeleteConv, proto::DeleteConvReq{conv_id});
    conv_svc_->HandleDeleteConv(alice.conn, del_pkt);

    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});

    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto alice_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    EXPECT_TRUE(alice_ack->conversations.empty());

    conv_svc_->HandleGetConvList(bob.conn, list_pkt);
    auto bob_ack = proto::Deserialize<proto::GetConvListAck>(bob.conn->last_pkt.body);
    ASSERT_EQ(bob_ack->conversations.size(), 1);
}

TEST_F(ConvServiceTest, MuteConvPerUser) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto conv_id = MakeFriends(alice, bob);

    // Alice 开启免打扰，Bob 不受影响
    auto mute_pkt = MakePacket(Cmd::kMuteConv, proto::MuteConvReq{conv_id, 1});
    conv_svc_->HandleMuteConv(alice.conn, mute_pkt);

    auto list_pkt = MakePacket(Cmd::kGetConvList, proto::GetConvListReq{});

    conv_svc_->HandleGetConvList(alice.conn, list_pkt);
    auto alice_ack = proto::Deserialize<proto::GetConvListAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(alice_ack->conversations[0].mute, 1);

    conv_svc_->HandleGetConvList(bob.conn, list_pkt);
    auto bob_ack = proto::Deserialize<proto::GetConvListAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(bob_ack->conversations[0].mute, 0);
}

}  // namespace
}  // namespace nova
