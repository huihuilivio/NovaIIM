#include "group_service.h"
#include "conv_service.h"
#include "../core/logger.h"
#include "../core/defer.h"
#include "../dao/conversation_dao.h"
#include "../dao/group_dao.h"
#include "../dao/user_dao.h"
#include <nova/errors.h>

#include <algorithm>
#include <chrono>
#include <ctime>

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "GroupSvc";

namespace {

inline bool ContainsControlChars(const std::string& s) {
    return std::any_of(s.begin(), s.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7F;
    });
}
inline std::string NowUtcStr() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    struct tm tm_buf {};
#ifdef _MSC_VER
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}
}  // namespace

static constexpr size_t kMaxGroupMembers = 500;

void GroupService::SendGroupNotify(int64_t conversation_id, int64_t exclude_user_id,
                                   const proto::GroupNotifyMsg& notify) {
    auto members = ctx_.dao().Conversation().GetMembersByConversation(conversation_id);
    Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(Cmd::kGroupNotify);
    pkt.seq  = 0;
    pkt.uid  = 0;
    pkt.body = proto::Serialize(notify);

    for (const auto& m : members) {
        if (m.user_id == exclude_user_id)
            continue;
        auto conns = ctx_.conn_manager().GetConns(m.user_id);
        for (auto& c : conns) {
            c->Send(pkt);
            ctx_.incr_messages_out();
        }
    }
}

// ---- CreateGroup ----

void GroupService::HandleCreateGroup(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::CreateGroupReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    // 校验群名
    if (req->name.empty()) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::group::kNameRequired.code, ec::group::kNameRequired.msg});
        return;
    }
    if (req->name.size() > 100) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::group::kNameTooLong.code, ec::group::kNameTooLong.msg});
        return;
    }
    if (ContainsControlChars(req->name)) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::group::kNameRequired.code, "group name contains invalid characters"});
        return;
    }
    if (req->avatar.size() > 512) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kInvalidBody.code, "avatar path too long"});
        return;
    }

    // 去重 member_ids 并移除自己
    {
        std::sort(req->member_ids.begin(), req->member_ids.end());
        req->member_ids.erase(std::unique(req->member_ids.begin(), req->member_ids.end()), req->member_ids.end());
        req->member_ids.erase(std::remove(req->member_ids.begin(), req->member_ids.end(), user_id), req->member_ids.end());
    }

    // 至少 2 个初始成员（不含自己）
    if (req->member_ids.size() < 2) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::group::kNotEnoughMembers.code, ec::group::kNotEnoughMembers.msg});
        return;
    }

    // 群成员上限（+1 算上群主自己）
    if (req->member_ids.size() + 1 > kMaxGroupMembers) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kInvalidBody.code, "group exceeds max member limit"});
        return;
    }

    // 校验所有 member_ids 对应的用户存在且非封禁
    {
        auto valid_users = ctx_.dao().User().FindByIds(req->member_ids);
        if (valid_users.size() != req->member_ids.size()) {
            SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                       proto::CreateGroupAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
            return;
        }
        for (const auto& u : valid_users) {
            if (u.status == static_cast<int>(AccountStatus::Banned)) {
                SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                           proto::CreateGroupAck{ec::user::kUserNotFound.code, "target user is banned"});
                return;
            }
        }
    }

    auto now = NowUtcStr();

    // 1. 创建群聊会话
    // 事务保证原子性
    if (!ctx_.dao().BeginTransaction()) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    bool committed = false;
    NOVA_DEFER {
        if (!committed) ctx_.dao().Rollback();
    };

    Conversation conv;
    conv.type       = static_cast<int>(ConvType::kGroup);
    conv.name       = req->name;
    conv.avatar     = req->avatar;
    conv.owner_id   = user_id;
    conv.created_at = now;
    conv.updated_at = now;

    if (!ctx_.dao().Conversation().CreateConversation(conv)) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // 2. 创建 groups 记录
    Group group;
    group.conversation_id = conv.id;
    group.name            = req->name;
    group.avatar          = req->avatar;
    group.owner_id        = user_id;
    group.created_at      = now;

    if (!ctx_.dao().Group().InsertGroup(group)) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // 3. 添加群主为成员
    ConversationMember owner_member;
    owner_member.conversation_id = conv.id;
    owner_member.user_id         = user_id;
    owner_member.role            = static_cast<int>(MemberRole::Owner);
    owner_member.joined_at       = now;
    if (!ctx_.dao().Conversation().AddMember(owner_member)) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // 4. 添加初始成员（已去重且已排除自己）
    for (auto mid : req->member_ids) {
        ConversationMember m;
        m.conversation_id = conv.id;
        m.user_id         = mid;
        m.role            = static_cast<int>(MemberRole::Member);
        m.joined_at       = now;
        if (!ctx_.dao().Conversation().AddMember(m)) {
            SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                       proto::CreateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
    }

    if (!ctx_.dao().Commit()) {
        SendPacket(conn, Cmd::kCreateGroupAck, seq, 0,
                   proto::CreateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    committed = true;

    // 5. 响应
    proto::CreateGroupAck ack;
    ack.code            = ec::kOk.code;
    ack.msg             = ec::kOk.msg;
    ack.conversation_id = conv.id;
    ack.group_id        = group.id;
    SendPacket(conn, Cmd::kCreateGroupAck, seq, 0, ack);

    // 6. 通知成员
    proto::GroupNotifyMsg notify;
    notify.conversation_id = conv.id;
    notify.notify_type     = 1;  // 创建
    notify.operator_id     = user_id;
    notify.target_ids      = req->member_ids;
    SendGroupNotify(conv.id, user_id, notify);

    NOVA_NLOG_INFO(kLogTag, "group created: conv={}, owner={}, members={}", conv.id, user_id, req->member_ids.size());
}

// ---- DismissGroup ----

void GroupService::HandleDismissGroup(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
                   proto::DismissGroupAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::DismissGroupReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
                   proto::DismissGroupAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto group = ctx_.dao().Group().FindByConversationId(req->conversation_id);
    if (!group) {
        SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
                   proto::DismissGroupAck{ec::group::kGroupNotFound.code, ec::group::kGroupNotFound.msg});
        return;
    }

    if (group->owner_id != user_id) {
        SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
                   proto::DismissGroupAck{ec::group::kNotOwner.code, ec::group::kNotOwner.msg});
        return;
    }

    // 在删除前获取成员列表（用于后续通知）
    auto members_snapshot = ctx_.dao().Conversation().GetMembersByConversation(req->conversation_id);

    // 删除群记录和会话成员（保留会话和消息记录，方便历史查看）— 事务保证原子性
    if (!ctx_.dao().BeginTransaction()) {
        SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
                   proto::DismissGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    bool committed = false;
    NOVA_DEFER {
        if (!committed) ctx_.dao().Rollback();
    };

    if (!ctx_.dao().Group().DeleteByConversationId(req->conversation_id)) {
        SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
                   proto::DismissGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    if (!ctx_.dao().Conversation().RemoveAllMembers(req->conversation_id)) {
        SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
                   proto::DismissGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    if (!ctx_.dao().Commit()) {
        SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
                   proto::DismissGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    committed = true;

    // 删除成功后通知所有原成员
    {
        proto::GroupNotifyMsg notify;
        notify.conversation_id = req->conversation_id;
        notify.notify_type     = 2;  // 解散
        notify.operator_id     = user_id;

        Packet npkt;
        npkt.cmd  = static_cast<uint16_t>(Cmd::kGroupNotify);
        npkt.seq  = 0;
        npkt.uid  = 0;
        npkt.body = proto::Serialize(notify);

        for (const auto& m : members_snapshot) {
            if (m.user_id == user_id) continue;
            auto conns = ctx_.conn_manager().GetConns(m.user_id);
            for (auto& c : conns) {
                c->Send(npkt);
                ctx_.incr_messages_out();
            }
        }
    }

    // ConvUpdate 通知（使用缓存的成员列表）
    {
        proto::ConvUpdateMsg update;
        update.conversation_id = req->conversation_id;
        update.update_type     = 4;  // 会话解散

        Packet upkt;
        upkt.cmd  = static_cast<uint16_t>(Cmd::kConvUpdate);
        upkt.seq  = 0;
        upkt.uid  = 0;
        upkt.body = proto::Serialize(update);

        for (const auto& m : members_snapshot) {
            if (m.user_id == user_id) continue;
            auto conns = ctx_.conn_manager().GetConns(m.user_id);
            for (auto& c : conns) {
                c->Send(upkt);
                ctx_.incr_messages_out();
            }
        }
    }

    SendPacket(conn, Cmd::kDismissGroupAck, seq, 0,
               proto::DismissGroupAck{ec::kOk.code, ec::kOk.msg});

    NOVA_NLOG_INFO(kLogTag, "group dismissed: conv={}, owner={}", req->conversation_id, user_id);
}

// ---- JoinGroup ----

void GroupService::HandleJoinGroup(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kJoinGroupAck, seq, 0,
                   proto::JoinGroupAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::JoinGroupReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kJoinGroupAck, seq, 0,
                   proto::JoinGroupAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto group = ctx_.dao().Group().FindByConversationId(req->conversation_id);
    if (!group) {
        SendPacket(conn, Cmd::kJoinGroupAck, seq, 0,
                   proto::JoinGroupAck{ec::group::kGroupNotFound.code, ec::group::kGroupNotFound.msg});
        return;
    }

    // 检查是否已是成员
    if (ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
        SendPacket(conn, Cmd::kJoinGroupAck, seq, 0,
                   proto::JoinGroupAck{ec::group::kAlreadyMember.code, ec::group::kAlreadyMember.msg});
        return;
    }

    // 检查是否有待处理的请求
    auto pending = ctx_.dao().Group().FindPendingJoinRequest(req->conversation_id, user_id);
    if (pending) {
        SendPacket(conn, Cmd::kJoinGroupAck, seq, 0,
                   proto::JoinGroupAck{ec::group::kRequestPending.code, ec::group::kRequestPending.msg});
        return;
    }

    // 创建加群申请
    if (req->remark.size() > 200) {
        SendPacket(conn, Cmd::kJoinGroupAck, seq, 0,
                   proto::JoinGroupAck{ec::kInvalidBody.code, "remark too long"});
        return;
    }

    GroupJoinRequest jr;
    jr.conversation_id = req->conversation_id;
    jr.user_id         = user_id;
    jr.remark          = req->remark;
    if (!ctx_.dao().Group().InsertJoinRequest(jr)) {
        SendPacket(conn, Cmd::kJoinGroupAck, seq, 0,
                   proto::JoinGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    SendPacket(conn, Cmd::kJoinGroupAck, seq, 0,
               proto::JoinGroupAck{ec::kOk.code, ec::kOk.msg});

    NOVA_NLOG_INFO(kLogTag, "join request: conv={}, user={}", req->conversation_id, user_id);
}

// ---- HandleJoinReq (管理员/群主审批) ----

void GroupService::HandleJoinReq(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                   proto::HandleJoinReqAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::HandleJoinReqReq>(pkt.body);
    if (!req || req->request_id <= 0 || (req->action != 1 && req->action != 2)) {
        SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                   proto::HandleJoinReqAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto jr = ctx_.dao().Group().FindJoinRequestById(req->request_id);
    if (!jr) {
        SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                   proto::HandleJoinReqAck{ec::group::kRequestNotFound.code, ec::group::kRequestNotFound.msg});
        return;
    }

    // 必须是待处理状态
    if (jr->status != static_cast<int>(JoinRequestStatus::Pending)) {
        SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                   proto::HandleJoinReqAck{ec::group::kRequestNotFound.code, ec::group::kRequestNotFound.msg});
        return;
    }

    // 操作者必须是管理员或群主
    auto member = ctx_.dao().Conversation().FindMember(jr->conversation_id, user_id);
    if (!member || (member->role != static_cast<int>(MemberRole::Owner) &&
                    member->role != static_cast<int>(MemberRole::Admin))) {
        SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                   proto::HandleJoinReqAck{ec::group::kNotAdminOrOwner.code, ec::group::kNotAdminOrOwner.msg});
        return;
    }

    // 如果接受：事务包裹 AddMember + UpdateJoinRequestStatus
    if (req->action == 1) {
        // 检查申请者是否已被封禁
        auto applicant = ctx_.dao().User().FindById(jr->user_id);
        if (!applicant || applicant->status == static_cast<int>(AccountStatus::Banned)) {
            SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                       proto::HandleJoinReqAck{ec::user::kUserNotFound.code, "applicant is banned or deleted"});
            return;
        }

        // 检查群成员上限
        auto member_count = ctx_.dao().Conversation().CountMembers(jr->conversation_id);
        if (member_count >= static_cast<int64_t>(kMaxGroupMembers)) {
            SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                       proto::HandleJoinReqAck{ec::kInvalidBody.code, "group is full"});
            return;
        }

        if (!ctx_.dao().BeginTransaction()) {
            SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                       proto::HandleJoinReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
        bool committed = false;
        NOVA_DEFER {
            if (!committed) ctx_.dao().Rollback();
        };

        auto now = NowUtcStr();
        ConversationMember m;
        m.conversation_id = jr->conversation_id;
        m.user_id         = jr->user_id;
        m.role            = static_cast<int>(MemberRole::Member);
        m.joined_at       = now;
        if (!ctx_.dao().Conversation().AddMember(m)) {
            SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                       proto::HandleJoinReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }

        if (!ctx_.dao().Group().UpdateJoinRequestStatus(req->request_id,
                                                   static_cast<int>(JoinRequestStatus::Accepted))) {
            SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                       proto::HandleJoinReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }

        if (!ctx_.dao().Commit()) {
            SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                       proto::HandleJoinReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
        committed = true;

        // 通知
        proto::GroupNotifyMsg notify;
        notify.conversation_id = jr->conversation_id;
        notify.notify_type     = 3;  // 加入
        notify.operator_id     = user_id;
        notify.target_ids.push_back(jr->user_id);
        SendGroupNotify(jr->conversation_id, 0, notify);

        proto::ConvUpdateMsg update;
        update.conversation_id = jr->conversation_id;
        update.update_type     = 2;  // 成员变化
        BroadcastConvUpdate(ctx_, jr->conversation_id, 0, update);
    } else {
        if (!ctx_.dao().Group().UpdateJoinRequestStatus(req->request_id,
                                                   static_cast<int>(JoinRequestStatus::Rejected))) {
            SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
                       proto::HandleJoinReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
    }

    SendPacket(conn, Cmd::kHandleJoinReqAck, seq, 0,
               proto::HandleJoinReqAck{ec::kOk.code, ec::kOk.msg});

    NOVA_NLOG_INFO(kLogTag, "handle join req: id={}, action={}", req->request_id, req->action);
}

// ---- LeaveGroup ----

void GroupService::HandleLeaveGroup(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kLeaveGroupAck, seq, 0,
                   proto::LeaveGroupAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::LeaveGroupReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kLeaveGroupAck, seq, 0,
                   proto::LeaveGroupAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto member = ctx_.dao().Conversation().FindMember(req->conversation_id, user_id);
    if (!member) {
        SendPacket(conn, Cmd::kLeaveGroupAck, seq, 0,
                   proto::LeaveGroupAck{ec::group::kNotMember.code, ec::group::kNotMember.msg});
        return;
    }

    // 群主不能直接退出
    if (member->role == static_cast<int>(MemberRole::Owner)) {
        SendPacket(conn, Cmd::kLeaveGroupAck, seq, 0,
                   proto::LeaveGroupAck{ec::group::kOwnerCannotLeave.code, ec::group::kOwnerCannotLeave.msg});
        return;
    }

    if (!ctx_.dao().Conversation().RemoveMember(req->conversation_id, user_id)) {
        SendPacket(conn, Cmd::kLeaveGroupAck, seq, 0,
                   proto::LeaveGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // 通知
    proto::GroupNotifyMsg notify;
    notify.conversation_id = req->conversation_id;
    notify.notify_type     = 4;  // 退出
    notify.operator_id     = user_id;
    notify.target_ids.push_back(user_id);
    SendGroupNotify(req->conversation_id, user_id, notify);

    proto::ConvUpdateMsg update;
    update.conversation_id = req->conversation_id;
    update.update_type     = 2;  // 成员变化
    BroadcastConvUpdate(ctx_, req->conversation_id, user_id, update);

    SendPacket(conn, Cmd::kLeaveGroupAck, seq, 0,
               proto::LeaveGroupAck{ec::kOk.code, ec::kOk.msg});

    NOVA_NLOG_INFO(kLogTag, "member left: conv={}, user={}", req->conversation_id, user_id);
}

// ---- KickMember ----

void GroupService::HandleKickMember(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kKickMemberAck, seq, 0,
                   proto::KickMemberAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::KickMemberReq>(pkt.body);
    if (!req || req->conversation_id <= 0 || req->target_user_id <= 0) {
        SendPacket(conn, Cmd::kKickMemberAck, seq, 0,
                   proto::KickMemberAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->target_user_id == user_id) {
        SendPacket(conn, Cmd::kKickMemberAck, seq, 0,
                   proto::KickMemberAck{ec::group::kCannotKickSelf.code, ec::group::kCannotKickSelf.msg});
        return;
    }

    auto op_member = ctx_.dao().Conversation().FindMember(req->conversation_id, user_id);
    if (!op_member || (op_member->role != static_cast<int>(MemberRole::Owner) &&
                       op_member->role != static_cast<int>(MemberRole::Admin))) {
        SendPacket(conn, Cmd::kKickMemberAck, seq, 0,
                   proto::KickMemberAck{ec::group::kNotAdminOrOwner.code, ec::group::kNotAdminOrOwner.msg});
        return;
    }

    auto target_member = ctx_.dao().Conversation().FindMember(req->conversation_id, req->target_user_id);
    if (!target_member) {
        SendPacket(conn, Cmd::kKickMemberAck, seq, 0,
                   proto::KickMemberAck{ec::group::kNotMember.code, ec::group::kNotMember.msg});
        return;
    }

    // 不能踢更高权限的成员
    if (target_member->role >= op_member->role) {
        SendPacket(conn, Cmd::kKickMemberAck, seq, 0,
                   proto::KickMemberAck{ec::group::kCannotKickHigherRole.code, ec::group::kCannotKickHigherRole.msg});
        return;
    }

    if (!ctx_.dao().Conversation().RemoveMember(req->conversation_id, req->target_user_id)) {
        SendPacket(conn, Cmd::kKickMemberAck, seq, 0,
                   proto::KickMemberAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // 通知
    proto::GroupNotifyMsg notify;
    notify.conversation_id = req->conversation_id;
    notify.notify_type     = 5;  // 踢出
    notify.operator_id     = user_id;
    notify.target_ids.push_back(req->target_user_id);
    SendGroupNotify(req->conversation_id, 0, notify);

    // 单独通知被踢成员
    {
        Packet kick_pkt;
        kick_pkt.cmd  = static_cast<uint16_t>(Cmd::kGroupNotify);
        kick_pkt.seq  = 0;
        kick_pkt.uid  = 0;
        kick_pkt.body = proto::Serialize(notify);
        auto conns = ctx_.conn_manager().GetConns(req->target_user_id);
        for (auto& c : conns) {
            c->Send(kick_pkt);
            ctx_.incr_messages_out();
        }
    }

    proto::ConvUpdateMsg update;
    update.conversation_id = req->conversation_id;
    update.update_type     = 2;  // 成员变化
    BroadcastConvUpdate(ctx_, req->conversation_id, 0, update);

    SendPacket(conn, Cmd::kKickMemberAck, seq, 0,
               proto::KickMemberAck{ec::kOk.code, ec::kOk.msg});

    NOVA_NLOG_INFO(kLogTag, "member kicked: conv={}, by={}, target={}", req->conversation_id, user_id, req->target_user_id);
}

// ---- GetGroupInfo ----

void GroupService::HandleGetGroupInfo(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kGetGroupInfoAck, seq, 0,
                   proto::GetGroupInfoAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::GetGroupInfoReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kGetGroupInfoAck, seq, 0,
                   proto::GetGroupInfoAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto group = ctx_.dao().Group().FindByConversationId(req->conversation_id);
    if (!group) {
        SendPacket(conn, Cmd::kGetGroupInfoAck, seq, 0,
                   proto::GetGroupInfoAck{ec::group::kGroupNotFound.code, ec::group::kGroupNotFound.msg});
        return;
    }

    // 鉴权：仅成员可查看群信息
    if (!ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
        SendPacket(conn, Cmd::kGetGroupInfoAck, seq, 0,
                   proto::GetGroupInfoAck{ec::group::kNotMember.code, ec::group::kNotMember.msg});
        return;
    }

    int member_count = ctx_.dao().Conversation().CountMembers(req->conversation_id);

    proto::GetGroupInfoAck ack;
    ack.code            = ec::kOk.code;
    ack.msg             = ec::kOk.msg;
    ack.conversation_id = req->conversation_id;
    ack.name            = group->name;
    ack.avatar          = group->avatar;
    ack.owner_id        = group->owner_id;
    ack.notice          = group->notice;
    ack.member_count    = member_count;
    ack.created_at      = group->created_at;
    SendPacket(conn, Cmd::kGetGroupInfoAck, seq, 0, ack);
}

// ---- UpdateGroup ----

void GroupService::HandleUpdateGroup(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                   proto::UpdateGroupAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::UpdateGroupReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                   proto::UpdateGroupAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto member = ctx_.dao().Conversation().FindMember(req->conversation_id, user_id);
    if (!member || (member->role != static_cast<int>(MemberRole::Owner) &&
                    member->role != static_cast<int>(MemberRole::Admin))) {
        SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                   proto::UpdateGroupAck{ec::group::kNotAdminOrOwner.code, ec::group::kNotAdminOrOwner.msg});
        return;
    }

    auto group = ctx_.dao().Group().FindByConversationId(req->conversation_id);
    if (!group) {
        SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                   proto::UpdateGroupAck{ec::group::kGroupNotFound.code, ec::group::kGroupNotFound.msg});
        return;
    }

    bool changed = false;
    if (!req->name.empty()) {
        if (req->name.size() > 100) {
            SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                       proto::UpdateGroupAck{ec::group::kNameTooLong.code, ec::group::kNameTooLong.msg});
            return;
        }
        if (ContainsControlChars(req->name)) {
            SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                       proto::UpdateGroupAck{ec::group::kNameRequired.code, "group name contains invalid characters"});
            return;
        }
        group->name = req->name;
        changed = true;
    }
    if (!req->avatar.empty()) {
        if (req->avatar.size() > 512) {
            SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                       proto::UpdateGroupAck{ec::kInvalidBody.code, "avatar path too long"});
            return;
        }
        group->avatar = req->avatar;
        changed = true;
    }
    if (!req->notice.empty()) {
        if (req->notice.size() > 1000) {
            SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                       proto::UpdateGroupAck{ec::group::kNoticeTooLong.code, ec::group::kNoticeTooLong.msg});
            return;
        }
        if (req->notice == std::string(1, '\0')) {
            group->notice.clear();
        } else {
            group->notice = req->notice;
        }
        changed = true;
    }

    if (!changed) {
        SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                   proto::UpdateGroupAck{ec::group::kNothingToUpdate.code, ec::group::kNothingToUpdate.msg});
        return;
    }

    // 事务保证 Group + Conversation 原子更新
    if (!ctx_.dao().BeginTransaction()) {
        SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                   proto::UpdateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    bool committed = false;
    NOVA_DEFER {
        if (!committed) ctx_.dao().Rollback();
    };

    if (!ctx_.dao().Group().UpdateGroup(*group)) {
        SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                   proto::UpdateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // 同步更新 Conversation 的 name / avatar
    auto conv = ctx_.dao().Conversation().FindById(req->conversation_id);
    if (conv) {
        if (!req->name.empty())
            conv->name = group->name;
        if (!req->avatar.empty())
            conv->avatar = group->avatar;
        if (!ctx_.dao().Conversation().UpdateConversation(*conv)) {
            SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                       proto::UpdateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
    }

    if (!ctx_.dao().Commit()) {
        SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
                   proto::UpdateGroupAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    committed = true;

    // 通知
    proto::GroupNotifyMsg notify;
    notify.conversation_id = req->conversation_id;
    notify.notify_type     = 6;  // 信息变更
    notify.operator_id     = user_id;
    SendGroupNotify(req->conversation_id, user_id, notify);

    proto::ConvUpdateMsg update;
    update.conversation_id = req->conversation_id;
    update.update_type     = 3;  // 会话信息变更
    BroadcastConvUpdate(ctx_, req->conversation_id, user_id, update);

    SendPacket(conn, Cmd::kUpdateGroupAck, seq, 0,
               proto::UpdateGroupAck{ec::kOk.code, ec::kOk.msg});

    NOVA_NLOG_INFO(kLogTag, "group updated: conv={}, by={}", req->conversation_id, user_id);
}

// ---- GetGroupMembers ----

void GroupService::HandleGetGroupMembers(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kGetGroupMembersAck, seq, 0,
                   proto::GetGroupMembersAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::GetGroupMembersReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kGetGroupMembersAck, seq, 0,
                   proto::GetGroupMembersAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    // 鉴权：仅成员可查看群成员列表
    if (!ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
        SendPacket(conn, Cmd::kGetGroupMembersAck, seq, 0,
                   proto::GetGroupMembersAck{ec::group::kNotMember.code, ec::group::kNotMember.msg});
        return;
    }

    auto members = ctx_.dao().Conversation().GetMembersByConversation(req->conversation_id);

    // 批量查用户信息
    std::vector<int64_t> user_ids;
    user_ids.reserve(members.size());
    for (const auto& m : members) {
        user_ids.push_back(m.user_id);
    }
    auto users = ctx_.dao().User().FindByIds(user_ids);
    std::unordered_map<int64_t, const User*> user_map;
    for (const auto& u : users) {
        user_map[u.id] = &u;
    }

    proto::GetGroupMembersAck ack;
    ack.code = ec::kOk.code;
    ack.msg  = ec::kOk.msg;
    ack.members.reserve(members.size());

    for (const auto& m : members) {
        proto::GroupMemberItem item;
        item.user_id   = m.user_id;
        item.role      = m.role;
        item.joined_at = m.joined_at;
        auto it = user_map.find(m.user_id);
        if (it != user_map.end()) {
            item.uid      = it->second->uid;
            item.nickname = it->second->nickname;
            item.avatar   = it->second->avatar;
        }
        ack.members.push_back(std::move(item));
    }

    SendPacket(conn, Cmd::kGetGroupMembersAck, seq, 0, ack);
}

// ---- GetMyGroups ----

void GroupService::HandleGetMyGroups(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kGetMyGroupsAck, seq, 0,
                   proto::GetMyGroupsAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto groups = ctx_.dao().Group().FindGroupsByUser(user_id);

    // 批量获取所有群的成员列表（避免 N+1 查询）
    std::vector<int64_t> conv_ids;
    conv_ids.reserve(groups.size());
    for (const auto& g : groups) {
        conv_ids.push_back(g.conversation_id);
    }

    // 1 次查询获取所有相关 conversation_members，再按 conversation_id 分组统计
    std::unordered_map<int64_t, int> member_count_map;
    std::unordered_map<int64_t, int> my_role_map;
    for (auto cid : conv_ids) {
        member_count_map[cid] = 0;
        my_role_map[cid]      = 0;
    }
    // 使用 GetMembersByUser 已查过自己的 memberships；这里需要 count 和 role
    // 批量获取 count 需要逐个查（无 batch CountMembers API），但可以用已有的 GetMembersByConversation
    // 更简洁的方案：直接遍历 conv_ids，用 2 次 batch 查询替代 2N 次
    // 先批量拿 my_role（GetMembersByUser 已经有了）
    auto my_memberships = ctx_.dao().Conversation().GetMembersByUser(user_id);
    for (const auto& m : my_memberships) {
        my_role_map[m.conversation_id] = m.role;
    }
    // member_count 仍需逐个查询（无 batch API），但数据量小（用户群数有限）
    for (auto cid : conv_ids) {
        member_count_map[cid] = ctx_.dao().Conversation().CountMembers(cid);
    }

    proto::GetMyGroupsAck ack;
    ack.code = ec::kOk.code;
    ack.msg  = ec::kOk.msg;
    ack.groups.reserve(groups.size());

    for (const auto& g : groups) {
        proto::MyGroupItem item;
        item.conversation_id = g.conversation_id;
        item.name            = g.name;
        item.avatar          = g.avatar;
        item.member_count    = member_count_map[g.conversation_id];
        item.my_role         = my_role_map[g.conversation_id];
        ack.groups.push_back(std::move(item));
    }

    SendPacket(conn, Cmd::kGetMyGroupsAck, seq, 0, ack);
}

// ---- SetMemberRole ----

void GroupService::HandleSetMemberRole(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kSetMemberRoleAck, seq, 0,
                   proto::SetMemberRoleAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::SetMemberRoleReq>(pkt.body);
    if (!req || req->conversation_id <= 0 || req->target_user_id <= 0) {
        SendPacket(conn, Cmd::kSetMemberRoleAck, seq, 0,
                   proto::SetMemberRoleAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    // 只能设 0=成员 或 1=管理员，不能设 2=群主
    if (req->role != 0 && req->role != 1) {
        SendPacket(conn, Cmd::kSetMemberRoleAck, seq, 0,
                   proto::SetMemberRoleAck{ec::group::kInvalidRole.code, ec::group::kInvalidRole.msg});
        return;
    }

    auto op_member = ctx_.dao().Conversation().FindMember(req->conversation_id, user_id);
    if (!op_member || op_member->role != static_cast<int>(MemberRole::Owner)) {
        SendPacket(conn, Cmd::kSetMemberRoleAck, seq, 0,
                   proto::SetMemberRoleAck{ec::group::kNotOwner.code, ec::group::kNotOwner.msg});
        return;
    }

    auto target = ctx_.dao().Conversation().FindMember(req->conversation_id, req->target_user_id);
    if (!target) {
        SendPacket(conn, Cmd::kSetMemberRoleAck, seq, 0,
                   proto::SetMemberRoleAck{ec::group::kNotMember.code, ec::group::kNotMember.msg});
        return;
    }

    // 不能修改自己的角色
    if (req->target_user_id == user_id) {
        SendPacket(conn, Cmd::kSetMemberRoleAck, seq, 0,
                   proto::SetMemberRoleAck{ec::group::kCannotKickSelf.code, "cannot change own role"});
        return;
    }

    // 更新角色
    if (!ctx_.dao().Conversation().UpdateMemberRole(req->conversation_id, req->target_user_id, req->role)) {
        SendPacket(conn, Cmd::kSetMemberRoleAck, seq, 0,
                   proto::SetMemberRoleAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // Notify
    proto::GroupNotifyMsg notify;
    notify.conversation_id = req->conversation_id;
    notify.notify_type     = 7;  // 角色变更
    notify.operator_id     = user_id;
    notify.target_ids.push_back(req->target_user_id);
    SendGroupNotify(req->conversation_id, 0, notify);

    SendPacket(conn, Cmd::kSetMemberRoleAck, seq, 0,
               proto::SetMemberRoleAck{ec::kOk.code, ec::kOk.msg});

    NOVA_NLOG_INFO(kLogTag, "member role changed: conv={}, target={}, role={}", req->conversation_id, req->target_user_id, req->role);
}

}  // namespace nova
