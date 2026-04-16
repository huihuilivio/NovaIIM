#include "sync_service.h"
#include "../core/server_context.h"
#include "../core/logger.h"
#include "../dao/conversation_dao.h"
#include "../dao/message_dao.h"

namespace nova {

static constexpr const char* kLogTag = "SyncService";
static constexpr int kDefaultSyncLimit = 20;
static constexpr int kMaxSyncLimit = 100;

template <typename T>
void SyncService::SendPacket(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const T& body) {
    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(cmd);
    pkt.seq = seq;
    pkt.uid = uid;
    pkt.body = proto::Serialize(body);
    conn->Send(pkt);
    ctx_.incr_messages_out();
}

void SyncService::HandleSyncMsg(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    auto uid = static_cast<uint64_t>(user_id);

    if (user_id == 0) {
        SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, 0, proto::SyncMsgResp{2});
        return;
    }

    // 1. 反序列化 body → SyncMsgReq
    auto req = proto::Deserialize<proto::SyncMsgReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, uid, proto::SyncMsgResp{1});
        return;
    }

    if (req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, uid, proto::SyncMsgResp{1});
        return;
    }

    int limit = req->limit;
    if (limit <= 0) limit = kDefaultSyncLimit;
    if (limit > kMaxSyncLimit) limit = kMaxSyncLimit;

    // 2. 从 DB 拉取 seq > last_seq 的消息
    auto messages = ctx_.dao().Message().GetAfterSeq(req->conversation_id, req->last_seq, limit);

    // 3. 构建响应
    proto::SyncMsgResp resp;
    resp.code = 0;
    resp.has_more = static_cast<int>(messages.size()) >= limit;
    resp.messages.reserve(messages.size());

    for (const auto& m : messages) {
        resp.messages.push_back({m.seq, m.sender_id, m.content,
                                 m.msg_type, m.created_at, m.status});
    }

    SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, uid, resp);

    NOVA_NLOG_DEBUG(kLogTag, "sync_msg: user={}, conv={}, from_seq={}, returned={}",
                    user_id, req->conversation_id, req->last_seq, messages.size());
}

void SyncService::HandleSyncUnread(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    auto uid = static_cast<uint64_t>(user_id);

    if (user_id == 0) {
        SendPacket(conn, Cmd::kSyncUnreadResp, pkt.seq, 0, proto::SyncUnreadResp{2});
        return;
    }

    // 1. 查询用户参与的所有会话
    auto memberships = ctx_.dao().Conversation().GetMembersByUser(user_id);

    // 2. 计算各会话未读数
    proto::SyncUnreadResp resp;
    resp.code = 0;

    for (const auto& member : memberships) {
        auto conv = ctx_.dao().Conversation().FindById(member.conversation_id);
        if (!conv) continue;

        int64_t unread = conv->max_seq - member.last_read_seq;
        if (unread < 0) unread = 0;
        resp.total_unread += unread;

        if (unread > 0) {
            proto::UnreadItem item;
            item.conversation_id = member.conversation_id;
            item.count = unread;

            // 拉取最近几条消息作为预览
            auto preview = ctx_.dao().Message().GetAfterSeq(
                member.conversation_id, conv->max_seq - 3, 3);
            for (const auto& m : preview) {
                item.latest_messages.push_back(
                    {m.seq, m.sender_id, m.content, m.msg_type, m.created_at, m.status});
            }

            resp.items.push_back(std::move(item));
        }
    }

    SendPacket(conn, Cmd::kSyncUnreadResp, pkt.seq, uid, resp);

    NOVA_NLOG_DEBUG(kLogTag, "sync_unread: user={}, conversations={}, total_unread={}",
                    user_id, memberships.size(), resp.total_unread);
}

} // namespace nova
