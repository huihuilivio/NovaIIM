#include "friend_service.h"
#include <nova/errors.h>
#include "../core/logger.h"
#include "../dao/friend_dao.h"
#include "../dao/user_dao.h"
#include "../dao/conversation_dao.h"

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
    }
}

// ---- handlers ----

void FriendService::HandleAddFriend(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    auto req = proto::Deserialize<proto::AddFriendReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->target_uid.empty()) {
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

    int64_t my_id     = conn->user_id();
    int64_t target_id = target->id;

    // 检查是否被对方拉黑
    auto blocked = ctx_.dao().Friend().FindFriendship(target_id, my_id);
    if (blocked && blocked->status == static_cast<int>(FriendshipStatus::Blocked)) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kBlockedByTarget.code, ec::friend_::kBlockedByTarget.msg});
        return;
    }

    // 检查是否已是好友
    auto existing = ctx_.dao().Friend().FindFriendship(my_id, target_id);
    if (existing && existing->status == static_cast<int>(FriendshipStatus::Normal)) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kAlreadyFriends.code, ec::friend_::kAlreadyFriends.msg});
        return;
    }

    // 检查是否有 pending 申请
    auto pending = ctx_.dao().Friend().FindPendingRequest(my_id, target_id);
    if (pending) {
        SendPacket(conn, Cmd::kAddFriendAck, seq, 0,
                   proto::AddFriendAck{ec::friend_::kRequestPending.code, ec::friend_::kRequestPending.msg});
        return;
    }

    // 插入申请
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

    ctx_.dao().Friend().UpdateRequestStatus(req->request_id, new_status);

    int64_t conversation_id = 0;

    if (req->action == 1) {
        // 同意：创建私聊会话 + 双向好友关系
        Conversation conv;
        conv.type = static_cast<int>(ConvType::kPrivate);
        if (ctx_.dao().Conversation().CreateConversation(conv)) {
            conversation_id = conv.id;

            ConversationMember m1;
            m1.conversation_id = conv.id;
            m1.user_id         = fr->from_id;
            ctx_.dao().Conversation().AddMember(m1);

            ConversationMember m2;
            m2.conversation_id = conv.id;
            m2.user_id         = fr->to_id;
            ctx_.dao().Conversation().AddMember(m2);

            // 双向写入 friendship
            Friendship f1;
            f1.user_id         = fr->from_id;
            f1.friend_id       = fr->to_id;
            f1.conversation_id = conv.id;
            f1.status          = static_cast<int>(FriendshipStatus::Normal);
            ctx_.dao().Friend().InsertFriendship(f1);

            Friendship f2;
            f2.user_id         = fr->to_id;
            f2.friend_id       = fr->from_id;
            f2.conversation_id = conv.id;
            f2.status          = static_cast<int>(FriendshipStatus::Normal);
            ctx_.dao().Friend().InsertFriendship(f2);

            NOVA_NLOG_INFO(kLogTag, "friend accepted: {} <-> {} (conv={})", fr->from_id, fr->to_id, conv.id);
        }
    } else {
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

    // 双向标记删除（保留历史消息）
    ctx_.dao().Friend().UpdateFriendshipStatus(my_id, target_id, static_cast<int>(FriendshipStatus::Deleted));
    ctx_.dao().Friend().UpdateFriendshipStatus(target_id, my_id, static_cast<int>(FriendshipStatus::Deleted));

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
        ctx_.dao().Friend().UpdateFriendshipStatus(my_id, target_id, static_cast<int>(FriendshipStatus::Blocked));
    } else {
        // 无记录（非好友直接拉黑）
        Friendship f;
        f.user_id   = my_id;
        f.friend_id = target_id;
        f.status    = static_cast<int>(FriendshipStatus::Blocked);
        ctx_.dao().Friend().InsertFriendship(f);
    }

    NOVA_NLOG_INFO(kLogTag, "user blocked: {} -> {}", conn->uid(), req->target_uid);

    SendPacket(conn, Cmd::kBlockFriendAck, seq, 0,
               proto::BlockFriendAck{ec::kOk.code, ec::kOk.msg});
}

void FriendService::HandleUnblock(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

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
    ctx_.dao().Friend().UpdateFriendshipStatus(my_id, target_id, static_cast<int>(FriendshipStatus::Deleted));

    NOVA_NLOG_INFO(kLogTag, "user unblocked: {} -> {}", conn->uid(), req->target_uid);

    SendPacket(conn, Cmd::kUnblockFriendAck, seq, 0,
               proto::UnblockFriendAck{ec::kOk.code, ec::kOk.msg});
}

void FriendService::HandleGetFriendList(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    auto friends = ctx_.dao().Friend().GetFriendsByUser(conn->user_id());

    proto::GetFriendListAck ack;
    ack.code = ec::kOk.code;
    ack.msg  = ec::kOk.msg;

    for (auto& f : friends) {
        auto user = ctx_.dao().User().FindById(f.friend_id);
        if (!user) continue;
        proto::FriendItem item;
        item.uid             = user->uid;
        item.nickname        = user->nickname;
        item.avatar          = user->avatar;
        item.conversation_id = f.conversation_id;
        ack.friends.push_back(std::move(item));
    }

    SendPacket(conn, Cmd::kGetFriendListAck, seq, 0, ack);
}

void FriendService::HandleGetRequests(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    proto::GetFriendRequestsReq req_body;
    auto parsed = proto::Deserialize<proto::GetFriendRequestsReq>(pkt.body);
    if (parsed) req_body = *parsed;

    if (req_body.page < 1) req_body.page = 1;
    if (req_body.page_size < 1) req_body.page_size = 20;
    if (req_body.page_size > 100) req_body.page_size = 100;

    int32_t offset = (req_body.page - 1) * req_body.page_size;
    auto page = ctx_.dao().Friend().GetRequestsByUser(conn->user_id(), offset, req_body.page_size);

    proto::GetFriendRequestsAck ack;
    ack.code  = ec::kOk.code;
    ack.msg   = ec::kOk.msg;
    ack.total = page.total;

    for (auto& r : page.items) {
        auto from_user = ctx_.dao().User().FindById(r.from_id);
        proto::FriendRequestItem item;
        item.request_id    = r.id;
        item.from_uid      = from_user ? from_user->uid : "";
        item.from_nickname = from_user ? from_user->nickname : "";
        item.from_avatar   = from_user ? from_user->avatar : "";
        item.remark        = r.message;
        item.created_at    = r.created_at;
        item.status        = r.status;
        ack.requests.push_back(std::move(item));
    }

    SendPacket(conn, Cmd::kGetFriendRequestsAck, seq, 0, ack);
}

}  // namespace nova
