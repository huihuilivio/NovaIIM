// test_msg_service.cpp — MsgService 单元测试
// 使用内存 SQLite + MockConnection 测试消息完整链路

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>

#include "core/app_config.h"
#include "core/server_context.h"
#include "dao/dao_factory.h"
#include "dao/user_dao.h"
#include "dao/conversation_dao.h"
#include <nova/packet.h>
#include <nova/protocol.h>
#include <nova/errors.h>
#include "net/connection.h"
#include "service/user_service.h"
#include "service/msg_service.h"
#include "service/friend_service.h"

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
        // Decode the encoded packet so tests can inspect it
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
class MsgServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        DatabaseConfig db_cfg;
        db_cfg.type = "sqlite";
        db_cfg.path = ":memory:";

        cfg_.server.recall_timeout_secs = 120;  // 2 分钟

        ctx_ = std::make_unique<ServerContext>(cfg_);
        ctx_->set_dao(CreateDaoFactory(db_cfg));
        user_svc_   = std::make_unique<UserService>(*ctx_);
        msg_svc_    = std::make_unique<MsgService>(*ctx_);
        friend_svc_ = std::make_unique<FriendService>(*ctx_);
    }

    struct UserInfo {
        std::shared_ptr<MockConnection> conn;
        std::string uid;
        int64_t user_id = 0;
    };

    UserInfo CreateAndLogin(const std::string& email, const std::string& nickname,
                            const std::string& device = "dev1") {
        // Register
        auto reg_conn = std::make_shared<MockConnection>();
        auto reg_pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{email, nickname, "password123"});
        user_svc_->HandleRegister(reg_conn, reg_pkt);
        auto reg_ack = proto::Deserialize<proto::RegisterAck>(reg_conn->last_pkt.body);
        EXPECT_TRUE(reg_ack.has_value());
        EXPECT_EQ(reg_ack->code, 0);

        // Login
        auto conn     = std::make_shared<MockConnection>();
        auto login_pkt = MakePacket(Cmd::kLogin, proto::LoginReq{email, "password123", device, "pc"});
        user_svc_->HandleLogin(conn, login_pkt);
        auto login_ack = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
        EXPECT_TRUE(login_ack.has_value());
        EXPECT_EQ(login_ack->code, 0);

        return {conn, login_ack->uid, conn->user_id()};
    }

    // 建立好友关系并返回 conversation_id
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

    AppConfig cfg_;
    std::unique_ptr<ServerContext> ctx_;
    std::unique_ptr<UserService> user_svc_;
    std::unique_ptr<MsgService> msg_svc_;
    std::unique_ptr<FriendService> friend_svc_;
};

// ================================================================
//  SendMsg
// ================================================================

TEST_F(MsgServiceTest, SendMsgSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "hello", proto::MsgType::kText, "cmid-1"});
    msg_svc_->HandleSendMsg(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_EQ(ack->server_seq, 1);
    EXPECT_GT(ack->server_time, 0);
}

TEST_F(MsgServiceTest, SendMsgSeqIncrement) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    // First message
    auto pkt1 = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "msg1", proto::MsgType::kText, "cmid-1"});
    msg_svc_->HandleSendMsg(alice.conn, pkt1);
    auto ack1 = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack1->server_seq, 1);

    // Second message
    auto pkt2 = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "msg2", proto::MsgType::kText, "cmid-2"});
    msg_svc_->HandleSendMsg(alice.conn, pkt2);
    auto ack2 = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack2->server_seq, 2);

    // Third message from Bob
    auto pkt3 = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "msg3", proto::MsgType::kText, "cmid-3"});
    msg_svc_->HandleSendMsg(bob.conn, pkt3);
    auto ack3 = proto::Deserialize<proto::SendMsgAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack3->server_seq, 3);
}

TEST_F(MsgServiceTest, SendMsgIdempotentDedup) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "hello", proto::MsgType::kText, "same-cmid"});

    // First send
    msg_svc_->HandleSendMsg(alice.conn, pkt);
    auto ack1 = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(ack1->code, 0);
    int64_t first_seq = ack1->server_seq;

    // Duplicate send with same client_msg_id
    msg_svc_->HandleSendMsg(alice.conn, pkt);
    auto ack2 = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack2->code, 0);
    EXPECT_EQ(ack2->server_seq, first_seq);  // Same seq, idempotent
}

TEST_F(MsgServiceTest, SendMsgEmptyContent) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 2001);  // kContentEmpty
}

TEST_F(MsgServiceTest, SendMsgInvalidConversation) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{0, "hi", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 2003);  // kInvalidConversation
}

TEST_F(MsgServiceTest, SendMsgNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    // Alice and Bob are friends
    int64_t conv_id = MakeFriends(alice, bob);

    // Carol tries to send to their conversation
    auto pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "intruder", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(carol.conn, pkt);
    auto ack = proto::Deserialize<proto::SendMsgAck>(carol.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 2005);  // kNotMember
}

TEST_F(MsgServiceTest, SendMsgNotAuthenticated) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{1, "hi", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(conn, pkt);
    auto ack = proto::Deserialize<proto::SendMsgAck>(conn->last_pkt.body);
    EXPECT_EQ(ack->code, -2);  // kNotAuthenticated
}

// ================================================================
//  RecallMsg
// ================================================================

TEST_F(MsgServiceTest, RecallMsgSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    // Send a message
    auto send_pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "to recall", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, send_pkt);
    auto send_ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(send_ack->code, 0);

    // Recall it
    auto recall_pkt = MakePacket(Cmd::kRecallMsg, proto::RecallMsgReq{conv_id, send_ack->server_seq});
    msg_svc_->HandleRecallMsg(alice.conn, recall_pkt);
    auto recall_ack = proto::Deserialize<proto::RecallMsgAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(recall_ack.has_value());
    EXPECT_EQ(recall_ack->code, 0);
}

TEST_F(MsgServiceTest, RecallMsgNotSender) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    // Alice sends
    auto send_pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "alice msg", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, send_pkt);
    auto send_ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(send_ack->code, 0);

    // Bob tries to recall Alice's message
    auto recall_pkt = MakePacket(Cmd::kRecallMsg, proto::RecallMsgReq{conv_id, send_ack->server_seq});
    msg_svc_->HandleRecallMsg(bob.conn, recall_pkt);
    auto recall_ack = proto::Deserialize<proto::RecallMsgAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(recall_ack->code, 2008);  // kRecallNoPermission
}

TEST_F(MsgServiceTest, RecallMsgNotFound) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kRecallMsg, proto::RecallMsgReq{conv_id, 999});
    msg_svc_->HandleRecallMsg(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::RecallMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 2006);  // kMsgNotFound
}

TEST_F(MsgServiceTest, RecallMsgAlreadyRecalled) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    auto send_pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "msg", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, send_pkt);
    auto send_ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);

    // Recall first time
    auto recall_pkt = MakePacket(Cmd::kRecallMsg, proto::RecallMsgReq{conv_id, send_ack->server_seq});
    msg_svc_->HandleRecallMsg(alice.conn, recall_pkt);
    auto ack1 = proto::Deserialize<proto::RecallMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack1->code, 0);

    // Recall again
    msg_svc_->HandleRecallMsg(alice.conn, recall_pkt);
    auto ack2 = proto::Deserialize<proto::RecallMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack2->code, 2009);  // kRecallAlready
}

TEST_F(MsgServiceTest, RecallMsgNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");
    int64_t conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kRecallMsg, proto::RecallMsgReq{conv_id, 1});
    msg_svc_->HandleRecallMsg(carol.conn, pkt);
    auto ack = proto::Deserialize<proto::RecallMsgAck>(carol.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 2005);  // kNotMember
}

TEST_F(MsgServiceTest, RecallMsgTimeout) {
    // Use very short timeout
    cfg_.server.recall_timeout_secs = 0;  // 0 seconds = immediate timeout

    ctx_ = std::make_unique<ServerContext>(cfg_);
    DatabaseConfig db_cfg;
    db_cfg.type = "sqlite";
    db_cfg.path = ":memory:";
    ctx_->set_dao(CreateDaoFactory(db_cfg));
    user_svc_   = std::make_unique<UserService>(*ctx_);
    msg_svc_    = std::make_unique<MsgService>(*ctx_);
    friend_svc_ = std::make_unique<FriendService>(*ctx_);

    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    auto send_pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "old msg", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, send_pkt);
    auto send_ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(send_ack->code, 0);

    // Wait a tiny bit to ensure time has passed (timeout = 0 seconds)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto recall_pkt = MakePacket(Cmd::kRecallMsg, proto::RecallMsgReq{conv_id, send_ack->server_seq});
    msg_svc_->HandleRecallMsg(alice.conn, recall_pkt);
    auto recall_ack = proto::Deserialize<proto::RecallMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(recall_ack->code, 2007);  // kRecallTimeout
}

TEST_F(MsgServiceTest, RecallMsgNotAuthenticated) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt = MakePacket(Cmd::kRecallMsg, proto::RecallMsgReq{1, 1});
    msg_svc_->HandleRecallMsg(conn, pkt);
    auto ack = proto::Deserialize<proto::RecallMsgAck>(conn->last_pkt.body);
    EXPECT_EQ(ack->code, -2);  // kNotAuthenticated
}

// ================================================================
//  DeliverAck
// ================================================================

TEST_F(MsgServiceTest, DeliverAckUpdatesSeq) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    // Alice sends message
    auto send_pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "hi", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, send_pkt);
    auto send_ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(send_ack->code, 0);

    // Bob confirms delivery
    auto ack_pkt = MakePacket(Cmd::kDeliverAck, proto::DeliverAckReq{conv_id, send_ack->server_seq});
    msg_svc_->HandleDeliverAck(bob.conn, ack_pkt);
    // DeliverAck is fire-and-forget, no response expected — just verify no crash
}

TEST_F(MsgServiceTest, DeliverAckNotAuthenticated) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt = MakePacket(Cmd::kDeliverAck, proto::DeliverAckReq{1, 1});
    msg_svc_->HandleDeliverAck(conn, pkt);
    // Should silently return (user_id == 0)
    EXPECT_EQ(conn->send_count, 0);
}

TEST_F(MsgServiceTest, DeliverAckInvalidBody) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(Cmd::kDeliverAck);
    pkt.seq  = 1;
    pkt.body = "garbage";
    msg_svc_->HandleDeliverAck(alice.conn, pkt);
    // Should silently return on bad body
}

// ================================================================
//  ReadAck
// ================================================================

TEST_F(MsgServiceTest, ReadAckSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    // Alice sends
    auto send_pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, "hi", proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, send_pkt);
    auto send_ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(send_ack->code, 0);

    // Bob marks read
    auto read_pkt = MakePacket(Cmd::kReadAck, proto::ReadAckReq{conv_id, send_ack->server_seq});
    msg_svc_->HandleReadAck(bob.conn, read_pkt);
    auto read_ack = proto::Deserialize<proto::RspBase>(bob.conn->last_pkt.body);
    ASSERT_TRUE(read_ack.has_value());
    EXPECT_EQ(read_ack->code, 0);
}

TEST_F(MsgServiceTest, ReadAckNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");
    int64_t conv_id = MakeFriends(alice, bob);

    auto pkt = MakePacket(Cmd::kReadAck, proto::ReadAckReq{conv_id, 1});
    msg_svc_->HandleReadAck(carol.conn, pkt);
    auto ack = proto::Deserialize<proto::RspBase>(carol.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 2005);  // kNotMember
}

TEST_F(MsgServiceTest, ReadAckInvalidSeq) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kReadAck, proto::ReadAckReq{0, 0});
    msg_svc_->HandleReadAck(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::RspBase>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, -1);  // kInvalidBody
}

TEST_F(MsgServiceTest, ReadAckNotAuthenticated) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt = MakePacket(Cmd::kReadAck, proto::ReadAckReq{1, 1});
    msg_svc_->HandleReadAck(conn, pkt);
    EXPECT_EQ(conn->send_count, 0);  // Silently returns
}

// ================================================================
//  Content size limit
// ================================================================

TEST_F(MsgServiceTest, SendMsgContentTooLarge) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    int64_t conv_id = MakeFriends(alice, bob);

    std::string big_content(cfg_.server.max_content_size + 1, 'x');
    auto pkt = MakePacket(Cmd::kSendMsg, proto::SendMsgReq{conv_id, big_content, proto::MsgType::kText, ""});
    msg_svc_->HandleSendMsg(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::SendMsgAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 2002);  // kContentTooLarge
}

}  // namespace
}  // namespace nova
