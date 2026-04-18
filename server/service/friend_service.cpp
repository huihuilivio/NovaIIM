#include "friend_service.h"
#include <nova/errors.h>
#include "../core/logger.h"
#include "../core/defer.h"
#include "../dao/friend_dao.h"
#include "../dao/user_dao.h"
#include "../dao/conversation_dao.h"

#include <unordered_map>

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "FriendSvc";

// ---- helpers ----

static void PushNotify(ServerContext& ctx, int64_t target_user_id,
                       int32_t notify_type, const User& from,
                       const std::string& remark, int64_t request_id,
                       int64_t conversation_id) {
    auto conns = ctx.conn_manager().GetConns(target_user_id);
    if (conns.empty()) return;

    proto::FriendNotifyMsg notify;
    notify.notify_type     = notify_type;
    notify.from_uid        = from.uid;
    notify.from_nickname   = from.nickname;
    notify.from_avatar     = from.avatar;
    notify.remark          = remark;
    notify.request_id      = request_id;
    notify.conversation_id = conversation_id;

    Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(Cmd::kFriendNotify);
    pkt.seq  = 0;
    pkt.uid  = 0;
    pkt.body = proto::Serialize(notify);

    for (auto& c : conns) {
        c->Send(pkt);
        ctx.incr_messages_out();
    }
}

// ---- handlers ----

void FriendService::HandleAddFriend(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    if (conn->user_id() == 0) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::AddFriendReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->target_uid.empty() || req->target_uid.size() > 255) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kTargetUidRequired.code, ec::friend_::kTargetUidRequired.msg});
        return;
    }

    // 不能加自己
    if (req->target_uid == conn->uid()) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kCannotAddSelf.code, ec::friend_::kCannotAddSelf.msg});
        return;
    }

    // 查找目标用户
    auto target = ctx_.dao().User().FindByUid(req->target_uid);
    if (!target) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
        return;
    }

    // 不能向已封禁用户发送好友请求
    if (target->status == static_cast<int>(AccountStatus::Banned)) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
        return;
    }

    int64_t my_id     = conn->user_id();
    int64_t target_id = target->id;

    // 检查是否被对方拉黑
    auto blocked = ctx_.dao().Friend().FindFriendship(target_id, my_id);
    if (blocked && blocked->status == static_cast<int>(FriendshipStatus::Blocked)) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kBlockedByTarget.code, ec::friend_::kBlockedByTarget.msg});
        return;
    }

    // 检查是否已是好友或已被自己拉黑
    auto existing = ctx_.dao().Friend().FindFriendship(my_id, target_id);
    if (existing && existing->status == static_cast<int>(FriendshipStatus::Normal)) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kAlreadyFriends.code, ec::friend_::kAlreadyFriends.msg});
        return;
    }
    if (existing && existing->status == static_cast<int>(FriendshipStatus::Blocked)) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kCannotAddSelf.code, "unblock target before adding"});
        return;
    }

    // 检查是否有 pending 申请（同方向）
    auto pending = ctx_.dao().Friend().FindPendingRequest(my_id, target_id);
    if (pending) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kRequestPending.code, ec::friend_::kRequestPending.msg});
        return;
    }

    // 检查是否有反方向的 pending 申请（对方已向我发起申请）
    auto reverse_pending = ctx_.dao().Friend().FindPendingRequest(target_id, my_id);
    if (reverse_pending) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kRequestPending.code, "target already sent you a request"});
        return;
    }

    // 插入申请
    if (req->remark.size() > 200) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::kInvalidBody.code, "remark too long"});
        return;
    }

    FriendRequest fr;
    fr.from_id = my_id;
    fr.to_id   = target_id;
    fr.message = req->remark;
    if (!ctx_.dao().Friend().InsertRequest(fr)) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    NOVA_NLOG_INFO(kLogTag, "friend request: {} -> {} (id={})", conn->uid(), req->target_uid, fr.id);

    // 推送通知给对方
    auto me = ctx_.dao().User().FindById(my_id);
    if (me) {
        PushNotify(ctx_, target_id, 1, *me, req->remark, fr.id, 0);
    }

    SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
               proto::AddFriendAck{ec::kOk.code, ec::kOk.msg, fr.id});
}

void FriendService::HandleRequest(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    if (conn->user_id() == 0) {
        SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                   proto::HandleFriendReqAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::HandleFriendReqReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                   proto::HandleFriendReqAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->request_id <= 0) {
        SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                   proto::HandleFriendReqAck{ec::friend_::kRequestIdRequired.code, ec::friend_::kRequestIdRequired.msg});
        return;
    }

    if (req->action != 1 && req->action != 2) {
        SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                   proto::HandleFriendReqAck{ec::friend_::kInvalidAction.code, ec::friend_::kInvalidAction.msg});
        return;
    }

    auto fr = ctx_.dao().Friend().FindRequestById(req->request_id);
    if (!fr || fr->to_id != conn->user_id()) {
        SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                   proto::HandleFriendReqAck{ec::friend_::kRequestNotFound.code, ec::friend_::kRequestNotFound.msg});
        return;
    }

    if (fr->status != static_cast<int>(FriendRequestStatus::Pending)) {
        SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                   proto::HandleFriendReqAck{ec::friend_::kRequestNotFound.code, ec::friend_::kRequestNotFound.msg});
        return;
    }

    int new_status = (req->action == 1) ? static_cast<int>(FriendRequestStatus::Accepted)
                                        : static_cast<int>(FriendRequestStatus::Rejected);

    int64_t conversation_id = 0;

    if (req->action == 1) {
        // 检查申请者是否仍有效（可能在发送请求后被封禁/删除）
        auto requester = ctx_.dao().User().FindById(fr->from_id);
        if (!requester || requester->status == static_cast<int>(AccountStatus::Banned)) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
            return;
        }

        // 同意：事务包裹 UpdateRequestStatus + 创建会话 + 双向好友关系
        if (!ctx_.dao().BeginTransaction()) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }

        bool committed = false;
        NOVA_DEFER {
            if (!committed) ctx_.dao().Rollback();
        };

        if (!ctx_.dao().Friend().UpdateRequestStatus(req->request_id, new_status)) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }

        Conversation conv;
        conv.type = static_cast<int>(ConvType::kPrivate);
        if (!ctx_.dao().Conversation().CreateConversation(conv)) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
        conversation_id = conv.id;

        ConversationMember m1;
        m1.conversation_id = conv.id;
        m1.user_id         = fr->from_id;
        if (!ctx_.dao().Conversation().AddMember(m1)) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }

        ConversationMember m2;
        m2.conversation_id = conv.id;
        m2.user_id         = fr->to_id;
        if (!ctx_.dao().Conversation().AddMember(m2)) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }

        // 双向写入 friendship
        Friendship f1;
        f1.user_id         = fr->from_id;
        f1.friend_id       = fr->to_id;
        f1.conversation_id = conv.id;
        f1.status          = static_cast<int>(FriendshipStatus::Normal);
        if (!ctx_.dao().Friend().InsertFriendship(f1)) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }

        Friendship f2;
        f2.user_id         = fr->to_id;
        f2.friend_id       = fr->from_id;
        f2.conversation_id = conv.id;
        f2.status          = static_cast<int>(FriendshipStatus::Normal);
        if (!ctx_.dao().Friend().InsertFriendship(f2)) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }

        if (!ctx_.dao().Commit()) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
        committed = true;

        NOVA_NLOG_INFO(kLogTag, "friend accepted: {} <-> {} (conv={})", fr->from_id, fr->to_id, conv.id);
    } else {
        if (!ctx_.dao().Friend().UpdateRequestStatus(req->request_id, new_status)) {
            SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
                       proto::HandleFriendReqAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
        NOVA_NLOG_INFO(kLogTag, "friend rejected: {} -> {}", fr->from_id, fr->to_id);
    }

    // 推送通知给申请发起方
    auto me = ctx_.dao().User().FindById(conn->user_id());
    if (me) {
        int notify_type = (req->action == 1) ? 2 : 3;
        PushNotify(ctx_, fr->from_id, notify_type, *me, "", fr->id, conversation_id);
    }

    SendPacket(conn, Cmd::kHandleFriendReqAck, seq, 0,
               proto::HandleFriendReqAck{ec::kOk.code, ec::kOk.msg, conversation_id});
}

void FriendService::HandleDeleteFriend(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    if (conn->user_id() == 0) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::DeleteFriendReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->target_uid.empty()) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::friend_::kTargetUidRequired.code, ec::friend_::kTargetUidRequired.msg});
        return;
    }

    auto target = ctx_.dao().User().FindByUid(req->target_uid);
    if (!target) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
        return;
    }

    int64_t my_id     = conn->user_id();
    int64_t target_id = target->id;

    auto fs = ctx_.dao().Friend().FindFriendship(my_id, target_id);
    if (!fs || fs->status != static_cast<int>(FriendshipStatus::Normal)) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::friend_::kNotFriends.code, ec::friend_::kNotFriends.msg});
        return;
    }

    // 双向标记删除（保留历史消息）— 事务保证原子性
    if (!ctx_.dao().BeginTransaction()) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    bool committed = false;
    NOVA_DEFER {
        if (!committed) ctx_.dao().Rollback();
    };

    if (!ctx_.dao().Friend().UpdateFriendshipStatus(my_id, target_id, static_cast<int>(FriendshipStatus::Deleted))) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    if (!ctx_.dao().Friend().UpdateFriendshipStatus(target_id, my_id, static_cast<int>(FriendshipStatus::Deleted))) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // 移除私聊会话成员，防止删除好友后仍可发消息
    if (fs->conversation_id > 0) {
        if (!ctx_.dao().Conversation().RemoveMember(fs->conversation_id, my_id)) {
            SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                       proto::DeleteFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
        if (!ctx_.dao().Conversation().RemoveMember(fs->conversation_id, target_id)) {
            SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                       proto::DeleteFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
    }

    if (!ctx_.dao().Commit()) {
        SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
                   proto::DeleteFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }
    committed = true;

    NOVA_NLOG_INFO(kLogTag, "friend deleted: {} <-> {}", conn->uid(), req->target_uid);

    // 推送通知
    auto me = ctx_.dao().User().FindById(my_id);
    if (me) {
        PushNotify(ctx_, target_id, 4, *me, "", 0, 0);
    }

    SendPacket(conn, Cmd::kDeleteFriendAck, seq, 0,
               proto::DeleteFriendAck{ec::kOk.code, ec::kOk.msg});
}

void FriendService::HandleBlock(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    if (conn->user_id() == 0) {
        SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
                   proto::BlockFriendAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::BlockFriendReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
                   proto::BlockFriendAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->target_uid.empty()) {
        SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
                   proto::BlockFriendAck{ec::friend_::kTargetUidRequired.code, ec::friend_::kTargetUidRequired.msg});
        return;
    }

    if (req->target_uid == conn->uid()) {
        SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
                   proto::BlockFriendAck{ec::friend_::kCannotAddSelf.code, "cannot block yourself"});
        return;
    }

    auto target = ctx_.dao().User().FindByUid(req->target_uid);
    if (!target) {
        SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
                   proto::BlockFriendAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
        return;
    }

    int64_t my_id     = conn->user_id();
    int64_t target_id = target->id;

    auto fs = ctx_.dao().Friend().FindFriendship(my_id, target_id);
    if (fs && fs->status == static_cast<int>(FriendshipStatus::Blocked)) {
        SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
                   proto::BlockFriendAck{ec::friend_::kAlreadyBlocked.code, ec::friend_::kAlreadyBlocked.msg});
        return;
    }

    if (fs) {
        // 已有记录（好友/已删除），更新为拉黑
        if (!ctx_.dao().Friend().UpdateFriendshipStatus(my_id, target_id, static_cast<int>(FriendshipStatus::Blocked))) {
            SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
                       proto::BlockFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
    } else {
        // 无记录（非好友直接拉黑）
        Friendship f;
        f.user_id   = my_id;
        f.friend_id = target_id;
        f.status    = static_cast<int>(FriendshipStatus::Blocked);
        if (!ctx_.dao().Friend().InsertFriendship(f)) {
            SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
                       proto::BlockFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
            return;
        }
    }

    NOVA_NLOG_INFO(kLogTag, "user blocked: {} -> {}", conn->uid(), req->target_uid);

    SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
               proto::BlockFriendAck{ec::kOk.code, ec::kOk.msg});
}

void FriendService::HandleUnblock(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    if (conn->user_id() == 0) {
        SendPacket(conn, Cmd::kUnblockFriendAck, seq, 0,
                   proto::UnblockFriendAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::UnblockFriendReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kUnblockFriendAck, seq, 0,
                   proto::UnblockFriendAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->target_uid.empty()) {
        SendPacket(conn, Cmd::kUnblockFriendAck, seq, 0,
                   proto::UnblockFriendAck{ec::friend_::kTargetUidRequired.code, ec::friend_::kTargetUidRequired.msg});
        return;
    }

    auto target = ctx_.dao().User().FindByUid(req->target_uid);
    if (!target) {
        SendPacket(conn, Cmd::kUnblockFriendAck, seq, 0,
                   proto::UnblockFriendAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
        return;
    }

    int64_t my_id     = conn->user_id();
    int64_t target_id = target->id;

    auto fs = ctx_.dao().Friend().FindFriendship(my_id, target_id);
    if (!fs || fs->status != static_cast<int>(FriendshipStatus::Blocked)) {
        SendPacket(conn, Cmd::kUnblockFriendAck, seq, 0,
                   proto::UnblockFriendAck{ec::friend_::kNotBlocked.code, ec::friend_::kNotBlocked.msg});
        return;
    }

    // 解除拉黑 → 设为已删除（不自动恢复好友）
    if (!ctx_.dao().Friend().UpdateFriendshipStatus(my_id, target_id, static_cast<int>(FriendshipStatus::Deleted))) {
        SendPacket(conn, Cmd::kUnblockFriendAck, seq, 0,
                   proto::UnblockFriendAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    NOVA_NLOG_INFO(kLogTag, "user unblocked: {} -> {}", conn->uid(), req->target_uid);

    SendPacket(conn, Cmd::kUnblockFriendAck, seq, 0,
               proto::UnblockFriendAck{ec::kOk.code, ec::kOk.msg});
}

void FriendService::HandleGetFriendList(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    if (conn->user_id() == 0) {
        SendPacket(conn, Cmd::kGetFriendListAck, seq, 0,
                   proto::GetFriendListAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto friends = ctx_.dao().Friend().GetFriendsByUser(conn->user_id());

    // 批量查询好友用户信息（1 次查询代替 N 次 FindById）
    std::vector<int64_t> friend_ids;
    friend_ids.reserve(friends.size());
    for (auto& f : friends) {
        friend_ids.push_back(f.friend_id);
    }
    auto users = ctx_.dao().User().FindByIds(friend_ids);

    std::unordered_map<int64_t, const User*> user_map;
    user_map.reserve(users.size());
    for (const auto& u : users) {
        user_map[u.id] = &u;
    }

    proto::GetFriendListAck ack;
    ack.code = ec::kOk.code;
    ack.msg  = ec::kOk.msg;

    for (auto& f : friends) {
        auto it = user_map.find(f.friend_id);
        if (it == user_map.end()) continue;
        const auto& user = *(it->second);
        proto::FriendItem item;
        item.uid             = user.uid;
        item.nickname        = user.nickname;
        item.avatar          = user.avatar;
        item.conversation_id = f.conversation_id;
        ack.friends.push_back(std::move(item));
    }

    SendPacket(conn, Cmd::kGetFriendListAck, seq, 0, ack);
}

void FriendService::HandleGetRequests(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    if (conn->user_id() == 0) {
        SendPacket(conn, Cmd::kGetFriendRequestsAck, seq, 0,
                   proto::GetFriendRequestsAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    proto::GetFriendRequestsReq req_body;
    auto parsed = proto::Deserialize<proto::GetFriendRequestsReq>(pkt.body);
    if (parsed) req_body = *parsed;

    if (req_body.page < 1) req_body.page = 1;
    if (req_body.page > 10000) req_body.page = 10000;
    if (req_body.page_size < 1) req_body.page_size = 20;
    if (req_body.page_size > 100) req_body.page_size = 100;

    int32_t offset = (req_body.page - 1) * req_body.page_size;
    auto page = ctx_.dao().Friend().GetRequestsByUser(conn->user_id(), offset, req_body.page_size);

    // 批量查询发起方用户信息（避免 N+1）
    std::vector<int64_t> from_ids;
    from_ids.reserve(page.items.size());
    for (const auto& r : page.items) {
        from_ids.push_back(r.from_id);
    }
    auto from_users = ctx_.dao().User().FindByIds(from_ids);
    std::unordered_map<int64_t, const User*> from_map;
    from_map.reserve(from_users.size());
    for (const auto& u : from_users) {
        from_map[u.id] = &u;
    }

    proto::GetFriendRequestsAck ack;
    ack.code  = ec::kOk.code;
    ack.msg   = ec::kOk.msg;
    ack.total = page.total;

    for (auto& r : page.items) {
        auto it = from_map.find(r.from_id);
        proto::FriendRequestItem item;
        item.request_id    = r.id;
        item.from_uid      = it != from_map.end() ? it->second->uid : "";
        item.from_nickname = it != from_map.end() ? it->second->nickname : "";
        item.from_avatar   = it != from_map.end() ? it->second->avatar : "";
        item.remark        = r.message;
        item.created_at    = r.created_at;
        item.status        = r.status;
        ack.requests.push_back(std::move(item));
    }

    SendPacket(conn, Cmd::kGetFriendRequestsAck, seq, 0, ack);
}

}  // namespace nova
