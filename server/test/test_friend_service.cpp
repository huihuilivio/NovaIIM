// test_friend_service.cpp — FriendService 单元测试
// 使用内存 SQLite + MockConnection 测试好友完整链路

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "core/app_config.h"
#include "core/server_context.h"
#include "dao/dao_factory.h"
#include "dao/user_dao.h"
#include <nova/packet.h>
#include <nova/protocol.h>
#include "net/connection.h"
#include "service/user_service.h"
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
    void SendEncoded(const std::string& /*data*/) override { ++send_count; }
    void Close() override { closed = true; }

    Packet last_pkt;
    int send_count = 0;
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
class FriendServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        DatabaseConfig db_cfg;
        db_cfg.type = "sqlite";
        db_cfg.path = ":memory:";

        ctx_ = std::make_unique<ServerContext>(cfg_);
        ctx_->set_dao(CreateDaoFactory(db_cfg));
        user_svc_   = std::make_unique<UserService>(*ctx_);
        friend_svc_ = std::make_unique<FriendService>(*ctx_);
    }

    // 注册 + 登录，返回已认证的连接
    struct UserInfo {
        std::shared_ptr<MockConnection> conn;
        std::string uid;
        int64_t user_id = 0;
    };

    UserInfo CreateAndLogin(const std::string& email, const std::string& nickname) {
        // Register
        auto reg_conn = std::make_shared<MockConnection>();
        auto reg_pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{email, nickname, "password123"});
        user_svc_->HandleRegister(reg_conn, reg_pkt);
        auto reg_ack = proto::Deserialize<proto::RegisterAck>(reg_conn->last_pkt.body);
        EXPECT_TRUE(reg_ack.has_value());
        EXPECT_EQ(reg_ack->code, 0);

        // Login
        auto conn     = std::make_shared<MockConnection>();
        auto login_pkt = MakePacket(Cmd::kLogin, proto::LoginReq{email, "password123", "dev1", "pc"});
        user_svc_->HandleLogin(conn, login_pkt);
        auto login_ack = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
        EXPECT_TRUE(login_ack.has_value());
        EXPECT_EQ(login_ack->code, 0);

        return {conn, login_ack->uid, conn->user_id()};
    }

    AppConfig cfg_;
    std::unique_ptr<ServerContext> ctx_;
    std::unique_ptr<UserService> user_svc_;
    std::unique_ptr<FriendService> friend_svc_;
};

// ================================================================
//  AddFriend
// ================================================================

TEST_F(FriendServiceTest, AddFriendSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, "hello!"});
    friend_svc_->HandleAddFriend(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_GT(ack->request_id, 0);
}

TEST_F(FriendServiceTest, AddFriendSelfFails) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{alice.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 5001);  // kCannotAddSelf
}

TEST_F(FriendServiceTest, AddFriendTargetNotFound) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{"nonexistent", ""});
    friend_svc_->HandleAddFriend(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 1019);  // kUserNotFound
}

TEST_F(FriendServiceTest, AddFriendDuplicateRequestPending) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto pkt1 = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, pkt1);
    auto ack1 = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack1->code, 0);

    auto pkt2 = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, pkt2);
    auto ack2 = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack2->code, 5003);  // kRequestPending
}

TEST_F(FriendServiceTest, AddFriendEmptyTargetUid) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{"", ""});
    friend_svc_->HandleAddFriend(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 5009);  // kTargetUidRequired
}

// ================================================================
//  HandleRequest (accept / reject)
// ================================================================

TEST_F(FriendServiceTest, AcceptRequestSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Alice sends request to Bob
    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, "hi"});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto add_ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(add_ack->code, 0);
    int64_t req_id = add_ack->request_id;

    // Bob accepts
    auto handle_pkt = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{req_id, 1});
    friend_svc_->HandleRequest(bob.conn, handle_pkt);
    auto handle_ack = proto::Deserialize<proto::HandleFriendReqAck>(bob.conn->last_pkt.body);
    ASSERT_TRUE(handle_ack.has_value());
    EXPECT_EQ(handle_ack->code, 0);
    EXPECT_GT(handle_ack->conversation_id, 0);
}

TEST_F(FriendServiceTest, RejectRequestSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto add_ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    int64_t req_id = add_ack->request_id;

    auto handle_pkt = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{req_id, 2});
    friend_svc_->HandleRequest(bob.conn, handle_pkt);
    auto handle_ack = proto::Deserialize<proto::HandleFriendReqAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(handle_ack->code, 0);
    EXPECT_EQ(handle_ack->conversation_id, 0);  // no conv created
}

TEST_F(FriendServiceTest, HandleRequestInvalidAction) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto add_ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);

    auto handle_pkt = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{add_ack->request_id, 99});
    friend_svc_->HandleRequest(bob.conn, handle_pkt);
    auto handle_ack = proto::Deserialize<proto::HandleFriendReqAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(handle_ack->code, 5011);  // kInvalidAction
}

TEST_F(FriendServiceTest, HandleRequestNotFound) {
    auto bob = CreateAndLogin("bob@test.com", "Bob");

    auto pkt = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{99999, 1});
    friend_svc_->HandleRequest(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::HandleFriendReqAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 5004);  // kRequestNotFound
}

TEST_F(FriendServiceTest, HandleRequestAlreadyProcessed) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto add_ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    int64_t req_id = add_ack->request_id;

    // Accept
    auto h1 = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{req_id, 1});
    friend_svc_->HandleRequest(bob.conn, h1);
    auto ack1 = proto::Deserialize<proto::HandleFriendReqAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack1->code, 0);

    // Try again
    auto h2 = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{req_id, 1});
    friend_svc_->HandleRequest(bob.conn, h2);
    auto ack2 = proto::Deserialize<proto::HandleFriendReqAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack2->code, 5004);  // kRequestNotFound (already processed)
}

// ================================================================
//  GetFriendList
// ================================================================

TEST_F(FriendServiceTest, GetFriendListAfterAccept) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Add + accept
    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto add_ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);

    auto h = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{add_ack->request_id, 1});
    friend_svc_->HandleRequest(bob.conn, h);

    // Alice's friend list
    auto list_pkt = MakePacket(Cmd::kGetFriendList, proto::GetFriendListReq{});
    friend_svc_->HandleGetFriendList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetFriendListAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(list_ack.has_value());
    EXPECT_EQ(list_ack->code, 0);
    ASSERT_EQ(list_ack->friends.size(), 1u);
    EXPECT_EQ(list_ack->friends[0].uid, bob.uid);
    EXPECT_EQ(list_ack->friends[0].nickname, "Bob");
    EXPECT_GT(list_ack->friends[0].conversation_id, 0);

    // Bob's friend list
    friend_svc_->HandleGetFriendList(bob.conn, list_pkt);
    auto bob_list = proto::Deserialize<proto::GetFriendListAck>(bob.conn->last_pkt.body);
    ASSERT_EQ(bob_list->friends.size(), 1u);
    EXPECT_EQ(bob_list->friends[0].uid, alice.uid);
}

TEST_F(FriendServiceTest, GetFriendListEmpty) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kGetFriendList, proto::GetFriendListReq{});
    friend_svc_->HandleGetFriendList(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::GetFriendListAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);
    EXPECT_TRUE(ack->friends.empty());
}

// ================================================================
//  DeleteFriend
// ================================================================

TEST_F(FriendServiceTest, DeleteFriendSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Become friends
    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto add_ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    auto h = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{add_ack->request_id, 1});
    friend_svc_->HandleRequest(bob.conn, h);

    // Delete
    auto del_pkt = MakePacket(Cmd::kDeleteFriend, proto::DeleteFriendReq{bob.uid});
    friend_svc_->HandleDeleteFriend(alice.conn, del_pkt);
    auto del_ack = proto::Deserialize<proto::DeleteFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(del_ack->code, 0);

    // Friend list should be empty now
    auto list_pkt = MakePacket(Cmd::kGetFriendList, proto::GetFriendListReq{});
    friend_svc_->HandleGetFriendList(alice.conn, list_pkt);
    auto list_ack = proto::Deserialize<proto::GetFriendListAck>(alice.conn->last_pkt.body);
    EXPECT_TRUE(list_ack->friends.empty());
}

TEST_F(FriendServiceTest, DeleteFriendNotFriends) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto pkt = MakePacket(Cmd::kDeleteFriend, proto::DeleteFriendReq{bob.uid});
    friend_svc_->HandleDeleteFriend(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::DeleteFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 5005);  // kNotFriends
}

// ================================================================
//  Block / Unblock
// ================================================================

TEST_F(FriendServiceTest, BlockUserSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto pkt = MakePacket(Cmd::kBlockFriend, proto::BlockFriendReq{bob.uid});
    friend_svc_->HandleBlock(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::BlockFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);
}

TEST_F(FriendServiceTest, BlockUserAlreadyBlocked) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto pkt = MakePacket(Cmd::kBlockFriend, proto::BlockFriendReq{bob.uid});
    friend_svc_->HandleBlock(alice.conn, pkt);

    // Block again
    friend_svc_->HandleBlock(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::BlockFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 5006);  // kAlreadyBlocked
}

TEST_F(FriendServiceTest, UnblockUserSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Block first
    auto block_pkt = MakePacket(Cmd::kBlockFriend, proto::BlockFriendReq{bob.uid});
    friend_svc_->HandleBlock(alice.conn, block_pkt);

    // Unblock
    auto unblock_pkt = MakePacket(Cmd::kUnblockFriend, proto::UnblockFriendReq{bob.uid});
    friend_svc_->HandleUnblock(alice.conn, unblock_pkt);
    auto ack = proto::Deserialize<proto::UnblockFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);
}

TEST_F(FriendServiceTest, UnblockUserNotBlocked) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto pkt = MakePacket(Cmd::kUnblockFriend, proto::UnblockFriendReq{bob.uid});
    friend_svc_->HandleUnblock(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UnblockFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 5007);  // kNotBlocked
}

TEST_F(FriendServiceTest, BlockPreventsAddFriend) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Bob blocks Alice
    auto block_pkt = MakePacket(Cmd::kBlockFriend, proto::BlockFriendReq{alice.uid});
    friend_svc_->HandleBlock(bob.conn, block_pkt);

    // Alice tries to add Bob
    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 5008);  // kBlockedByTarget
}

// ================================================================
//  GetFriendRequests
// ================================================================

TEST_F(FriendServiceTest, GetRequestsList) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Alice sends request to Bob
    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, "plz add me"});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);

    // Bob gets requests
    auto req_pkt = MakePacket(Cmd::kGetFriendRequests, proto::GetFriendRequestsReq{1, 20});
    friend_svc_->HandleGetRequests(bob.conn, req_pkt);
    auto ack = proto::Deserialize<proto::GetFriendRequestsAck>(bob.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_EQ(ack->total, 1);
    ASSERT_EQ(ack->requests.size(), 1u);
    EXPECT_EQ(ack->requests[0].from_uid, alice.uid);
    EXPECT_EQ(ack->requests[0].from_nickname, "Alice");
    EXPECT_EQ(ack->requests[0].remark, "plz add me");
    EXPECT_EQ(ack->requests[0].status, 0);  // pending
}

TEST_F(FriendServiceTest, GetRequestsEmpty) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kGetFriendRequests, proto::GetFriendRequestsReq{1, 20});
    friend_svc_->HandleGetRequests(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::GetFriendRequestsAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);
    EXPECT_EQ(ack->total, 0);
    EXPECT_TRUE(ack->requests.empty());
}

// ================================================================
//  AddFriend after already friends
// ================================================================

TEST_F(FriendServiceTest, AddFriendAlreadyFriends) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Become friends
    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto add_ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    auto h = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{add_ack->request_id, 1});
    friend_svc_->HandleRequest(bob.conn, h);

    // Try to add again
    auto pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 5002);  // kAlreadyFriends
}

// ================================================================
//  Conversation shared between friends
// ================================================================

TEST_F(FriendServiceTest, FriendsShareConversation) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Become friends
    auto add_pkt = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{bob.uid, ""});
    friend_svc_->HandleAddFriend(alice.conn, add_pkt);
    auto add_ack = proto::Deserialize<proto::AddFriendAck>(alice.conn->last_pkt.body);
    auto h = MakePacket(Cmd::kHandleFriendReq, proto::HandleFriendReqReq{add_ack->request_id, 1});
    friend_svc_->HandleRequest(bob.conn, h);
    auto h_ack = proto::Deserialize<proto::HandleFriendReqAck>(bob.conn->last_pkt.body);
    int64_t conv_id = h_ack->conversation_id;

    // Both should see the same conversation_id
    auto list_pkt = MakePacket(Cmd::kGetFriendList, proto::GetFriendListReq{});

    friend_svc_->HandleGetFriendList(alice.conn, list_pkt);
    auto alice_list = proto::Deserialize<proto::GetFriendListAck>(alice.conn->last_pkt.body);
    ASSERT_EQ(alice_list->friends.size(), 1u);
    EXPECT_EQ(alice_list->friends[0].conversation_id, conv_id);

    friend_svc_->HandleGetFriendList(bob.conn, list_pkt);
    auto bob_list = proto::Deserialize<proto::GetFriendListAck>(bob.conn->last_pkt.body);
    ASSERT_EQ(bob_list->friends.size(), 1u);
    EXPECT_EQ(bob_list->friends[0].conversation_id, conv_id);
}

// ================================================================
//  Unauthenticated access
// ================================================================

TEST_F(FriendServiceTest, AddFriendNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kAddFriend, proto::AddFriendReq{"some_uid", ""});
    friend_svc_->HandleAddFriend(conn, pkt);
    auto ack = proto::Deserialize<proto::AddFriendAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

TEST_F(FriendServiceTest, GetFriendListNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kGetFriendList, proto::GetFriendListReq{});
    friend_svc_->HandleGetFriendList(conn, pkt);
    auto ack = proto::Deserialize<proto::GetFriendListAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

TEST_F(FriendServiceTest, DeleteFriendNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kDeleteFriend, proto::DeleteFriendReq{"some_uid"});
    friend_svc_->HandleDeleteFriend(conn, pkt);
    auto ack = proto::Deserialize<proto::DeleteFriendAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

}  // namespace
}  // namespace nova
