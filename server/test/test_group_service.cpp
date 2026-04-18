// test_group_service.cpp — GroupService 单元测试
// 使用内存 SQLite + MockConnection 测试群组管理完整链路

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "core/app_config.h"
#include "core/server_context.h"
#include "dao/dao_factory.h"
#include "dao/conversation_dao.h"
#include "dao/group_dao.h"
#include <nova/packet.h>
#include <nova/protocol.h>
#include <nova/errors.h>
#include "net/connection.h"
#include "service/user_service.h"
#include "service/msg_service.h"
#include "service/friend_service.h"
#include "service/conv_service.h"
#include "service/group_service.h"

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
class GroupServiceTest : public ::testing::Test {
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
        group_svc_  = std::make_unique<GroupService>(*ctx_);
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

    // 创建群组：owner + member_ids
    struct CreateGroupResult {
        int64_t conversation_id = 0;
        int64_t group_id        = 0;
    };

    CreateGroupResult CreateGroup(UserInfo& owner, const std::string& name,
                                  const std::vector<int64_t>& member_ids) {
        auto pkt = MakePacket(Cmd::kCreateGroup,
                              proto::CreateGroupReq{name, "", member_ids});
        group_svc_->HandleCreateGroup(owner.conn, pkt);
        auto ack = proto::Deserialize<proto::CreateGroupAck>(owner.conn->last_pkt.body);
        EXPECT_TRUE(ack.has_value());
        EXPECT_EQ(ack->code, 0);
        return {ack->conversation_id, ack->group_id};
    }

    AppConfig cfg_;
    std::unique_ptr<ServerContext> ctx_;
    std::unique_ptr<UserService> user_svc_;
    std::unique_ptr<MsgService> msg_svc_;
    std::unique_ptr<FriendService> friend_svc_;
    std::unique_ptr<ConvService> conv_svc_;
    std::unique_ptr<GroupService> group_svc_;
};

// ================================================================
//  CreateGroup
// ================================================================

TEST_F(GroupServiceTest, CreateGroupSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});
    EXPECT_GT(result.conversation_id, 0);
    EXPECT_GT(result.group_id, 0);

    // 验证群信息
    auto pkt = MakePacket(Cmd::kGetGroupInfo,
                          proto::GetGroupInfoReq{result.conversation_id});
    group_svc_->HandleGetGroupInfo(alice.conn, pkt);
    auto info = proto::Deserialize<proto::GetGroupInfoAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->code, 0);
    EXPECT_EQ(info->name, "TestGroup");
    EXPECT_EQ(info->owner_id, alice.user_id);
    EXPECT_EQ(info->member_count, 3);
}

TEST_F(GroupServiceTest, CreateGroupNameRequired) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto pkt = MakePacket(Cmd::kCreateGroup,
                          proto::CreateGroupReq{"", "", {bob.user_id, carol.user_id}});
    group_svc_->HandleCreateGroup(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::CreateGroupAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNameRequired.code);
}

TEST_F(GroupServiceTest, CreateGroupNotEnoughMembers) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    auto pkt = MakePacket(Cmd::kCreateGroup,
                          proto::CreateGroupReq{"Grp", "", {bob.user_id}});
    group_svc_->HandleCreateGroup(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::CreateGroupAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNotEnoughMembers.code);
}

TEST_F(GroupServiceTest, CreateGroupNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kCreateGroup, proto::CreateGroupReq{"Grp", "", {1, 2}});
    group_svc_->HandleCreateGroup(conn, pkt);
    auto ack = proto::Deserialize<proto::CreateGroupAck>(conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::kNotAuthenticated.code);
}

// ================================================================
//  DismissGroup
// ================================================================

TEST_F(GroupServiceTest, DismissGroupSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kDismissGroup,
                          proto::DismissGroupReq{result.conversation_id});
    group_svc_->HandleDismissGroup(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::DismissGroupAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);

    // 群不存在了
    auto info_pkt = MakePacket(Cmd::kGetGroupInfo,
                               proto::GetGroupInfoReq{result.conversation_id});
    group_svc_->HandleGetGroupInfo(alice.conn, info_pkt);
    auto info = proto::Deserialize<proto::GetGroupInfoAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(info->code, errc::group::kGroupNotFound.code);
}

TEST_F(GroupServiceTest, DismissGroupNotOwner) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kDismissGroup,
                          proto::DismissGroupReq{result.conversation_id});
    group_svc_->HandleDismissGroup(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::DismissGroupAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNotOwner.code);
}

// ================================================================
//  LeaveGroup
// ================================================================

TEST_F(GroupServiceTest, LeaveGroupSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kLeaveGroup,
                          proto::LeaveGroupReq{result.conversation_id});
    group_svc_->HandleLeaveGroup(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::LeaveGroupAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);

    // 成员数减少
    auto info_pkt = MakePacket(Cmd::kGetGroupInfo,
                               proto::GetGroupInfoReq{result.conversation_id});
    group_svc_->HandleGetGroupInfo(alice.conn, info_pkt);
    auto info = proto::Deserialize<proto::GetGroupInfoAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(info->member_count, 2);
}

TEST_F(GroupServiceTest, LeaveGroupOwnerBlocked) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kLeaveGroup,
                          proto::LeaveGroupReq{result.conversation_id});
    group_svc_->HandleLeaveGroup(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::LeaveGroupAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kOwnerCannotLeave.code);
}

TEST_F(GroupServiceTest, LeaveGroupNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");
    auto dave  = CreateAndLogin("dave@test.com", "Dave");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kLeaveGroup,
                          proto::LeaveGroupReq{result.conversation_id});
    group_svc_->HandleLeaveGroup(dave.conn, pkt);
    auto ack = proto::Deserialize<proto::LeaveGroupAck>(dave.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNotMember.code);
}

// ================================================================
//  KickMember
// ================================================================

TEST_F(GroupServiceTest, KickMemberSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kKickMember,
                          proto::KickMemberReq{result.conversation_id, bob.user_id});
    group_svc_->HandleKickMember(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::KickMemberAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);

    // 成员数减少
    auto info_pkt = MakePacket(Cmd::kGetGroupInfo,
                               proto::GetGroupInfoReq{result.conversation_id});
    group_svc_->HandleGetGroupInfo(alice.conn, info_pkt);
    auto info = proto::Deserialize<proto::GetGroupInfoAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(info->member_count, 2);
}

TEST_F(GroupServiceTest, KickMemberNotAdmin) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});

    // bob 不是管理员，不能踢人
    auto pkt = MakePacket(Cmd::kKickMember,
                          proto::KickMemberReq{result.conversation_id, carol.user_id});
    group_svc_->HandleKickMember(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::KickMemberAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNotAdminOrOwner.code);
}

TEST_F(GroupServiceTest, KickMemberCannotKickSelf) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "TestGroup", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kKickMember,
                          proto::KickMemberReq{result.conversation_id, alice.user_id});
    group_svc_->HandleKickMember(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::KickMemberAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kCannotKickSelf.code);
}

// ================================================================
//  UpdateGroup
// ================================================================

TEST_F(GroupServiceTest, UpdateGroupName) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "OldName", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kUpdateGroup,
                          proto::UpdateGroupReq{result.conversation_id, "NewName", "", ""});
    group_svc_->HandleUpdateGroup(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UpdateGroupAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);

    // 验证
    auto info_pkt = MakePacket(Cmd::kGetGroupInfo,
                               proto::GetGroupInfoReq{result.conversation_id});
    group_svc_->HandleGetGroupInfo(alice.conn, info_pkt);
    auto info = proto::Deserialize<proto::GetGroupInfoAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(info->name, "NewName");
}

TEST_F(GroupServiceTest, UpdateGroupNothingToUpdate) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kUpdateGroup,
                          proto::UpdateGroupReq{result.conversation_id, "", "", ""});
    group_svc_->HandleUpdateGroup(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UpdateGroupAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNothingToUpdate.code);
}

TEST_F(GroupServiceTest, UpdateGroupNotAdmin) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kUpdateGroup,
                          proto::UpdateGroupReq{result.conversation_id, "X", "", ""});
    group_svc_->HandleUpdateGroup(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::UpdateGroupAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNotAdminOrOwner.code);
}

// ================================================================
//  GetGroupMembers
// ================================================================

TEST_F(GroupServiceTest, GetGroupMembersSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kGetGroupMembers,
                          proto::GetGroupMembersReq{result.conversation_id});
    group_svc_->HandleGetGroupMembers(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::GetGroupMembersAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_EQ(ack->members.size(), 3);
}

// ================================================================
//  GetMyGroups
// ================================================================

TEST_F(GroupServiceTest, GetMyGroupsSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    CreateGroup(alice, "Grp1", {bob.user_id, carol.user_id});
    CreateGroup(bob, "Grp2", {alice.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kGetMyGroups, proto::GetMyGroupsReq{});
    group_svc_->HandleGetMyGroups(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::GetMyGroupsAck>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_EQ(ack->groups.size(), 2);
}

TEST_F(GroupServiceTest, GetMyGroupsEmpty) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kGetMyGroups, proto::GetMyGroupsReq{});
    group_svc_->HandleGetMyGroups(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::GetMyGroupsAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);
    EXPECT_TRUE(ack->groups.empty());
}

// ================================================================
//  SetMemberRole
// ================================================================

TEST_F(GroupServiceTest, SetMemberRoleSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    // 提升 bob 为管理员
    auto pkt = MakePacket(Cmd::kSetMemberRole,
                          proto::SetMemberRoleReq{result.conversation_id, bob.user_id, 1});
    group_svc_->HandleSetMemberRole(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::SetMemberRoleAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);
}

TEST_F(GroupServiceTest, SetMemberRoleNotOwner) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kSetMemberRole,
                          proto::SetMemberRoleReq{result.conversation_id, carol.user_id, 1});
    group_svc_->HandleSetMemberRole(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::SetMemberRoleAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNotOwner.code);
}

TEST_F(GroupServiceTest, SetMemberRoleInvalid) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kSetMemberRole,
                          proto::SetMemberRoleReq{result.conversation_id, bob.user_id, 2});
    group_svc_->HandleSetMemberRole(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::SetMemberRoleAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kInvalidRole.code);
}

// ================================================================
//  JoinGroup + HandleJoinReq
// ================================================================

TEST_F(GroupServiceTest, JoinGroupFlow) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");
    auto dave  = CreateAndLogin("dave@test.com", "Dave");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    // dave 申请入群
    auto join_pkt = MakePacket(Cmd::kJoinGroup,
                               proto::JoinGroupReq{result.conversation_id, "hi"});
    group_svc_->HandleJoinGroup(dave.conn, join_pkt);
    auto join_ack = proto::Deserialize<proto::JoinGroupAck>(dave.conn->last_pkt.body);
    EXPECT_EQ(join_ack->code, 0);

    // alice 审批
    // 先取到 join_request_id (它是 1, 因为是第一个请求)
    auto handle_pkt = MakePacket(Cmd::kHandleJoinReq,
                                 proto::HandleJoinReqReq{1, 1});  // accept
    group_svc_->HandleJoinReq(alice.conn, handle_pkt);
    auto handle_ack = proto::Deserialize<proto::HandleJoinReqAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(handle_ack->code, 0);

    // 验证成员数变为 4
    auto info_pkt = MakePacket(Cmd::kGetGroupInfo,
                               proto::GetGroupInfoReq{result.conversation_id});
    group_svc_->HandleGetGroupInfo(alice.conn, info_pkt);
    auto info = proto::Deserialize<proto::GetGroupInfoAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(info->member_count, 4);
}

TEST_F(GroupServiceTest, JoinGroupAlreadyMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kJoinGroup,
                          proto::JoinGroupReq{result.conversation_id, ""});
    group_svc_->HandleJoinGroup(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::JoinGroupAck>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kAlreadyMember.code);
}

TEST_F(GroupServiceTest, JoinGroupDuplicateRequest) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");
    auto dave  = CreateAndLogin("dave@test.com", "Dave");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    auto pkt = MakePacket(Cmd::kJoinGroup,
                          proto::JoinGroupReq{result.conversation_id, ""});
    group_svc_->HandleJoinGroup(dave.conn, pkt);

    // 重复申请
    group_svc_->HandleJoinGroup(dave.conn, pkt);
    auto ack = proto::Deserialize<proto::JoinGroupAck>(dave.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kRequestPending.code);
}

// ================================================================
//  UpdateGroup name too long
// ================================================================

TEST_F(GroupServiceTest, UpdateGroupNameTooLong) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    std::string long_name(101, 'x');
    auto pkt = MakePacket(Cmd::kUpdateGroup,
                          proto::UpdateGroupReq{result.conversation_id, long_name, "", ""});
    group_svc_->HandleUpdateGroup(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UpdateGroupAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNameTooLong.code);
}

// ================================================================
//  Authorization: non-member access
// ================================================================

TEST_F(GroupServiceTest, GetGroupInfoNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");
    auto dave  = CreateAndLogin("dave@test.com", "Dave");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    // dave is not a member
    auto pkt = MakePacket(Cmd::kGetGroupInfo,
                          proto::GetGroupInfoReq{result.conversation_id});
    group_svc_->HandleGetGroupInfo(dave.conn, pkt);
    auto ack = proto::Deserialize<proto::GetGroupInfoAck>(dave.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNotMember.code);
}

TEST_F(GroupServiceTest, GetGroupMembersNotMember) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");
    auto carol = CreateAndLogin("carol@test.com", "Carol");
    auto dave  = CreateAndLogin("dave@test.com", "Dave");

    auto result = CreateGroup(alice, "Grp", {bob.user_id, carol.user_id});

    // dave is not a member
    auto pkt = MakePacket(Cmd::kGetGroupMembers,
                          proto::GetGroupMembersReq{result.conversation_id});
    group_svc_->HandleGetGroupMembers(dave.conn, pkt);
    auto ack = proto::Deserialize<proto::GetGroupMembersAck>(dave.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::group::kNotMember.code);
}

TEST_F(GroupServiceTest, CreateGroupInvalidMemberIds) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // 99999 doesn't exist
    auto pkt = MakePacket(Cmd::kCreateGroup,
                          proto::CreateGroupReq{"Grp", "", {bob.user_id, 99999}});
    group_svc_->HandleCreateGroup(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::CreateGroupAck>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::user::kUserNotFound.code);
}

}  // namespace
}  // namespace nova
