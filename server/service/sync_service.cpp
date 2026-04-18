#include "sync_service.h"
#include "errors/sync_errors.h"
#include "../core/logger.h"
#include "../dao/conversation_dao.h"
#include "../dao/message_dao.h"
#include "../dao/user_dao.h"

#include <unordered_map>

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "SyncService";

void SyncService::HandleSyncMsg(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();

    if (user_id == 0) {
        SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, 0, proto::SyncMsgResp{ec::kNotAuthenticated.code});
        return;
    }

    // 1. 反序列化 body → SyncMsgReq
    auto req = proto::Deserialize<proto::SyncMsgReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, 0, proto::SyncMsgResp{ec::kInvalidBody.code});
        return;
    }

    if (req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, 0, proto::SyncMsgResp{ec::kInvalidBody.code});
        return;
    }

    // 检查用户是否为会话成员
    if (!ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
        SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, 0, proto::SyncMsgResp{ec::sync::kNotMember.code});
        return;
    }

    const int sync_default = ctx_.config().server.sync_default;
    const int sync_max     = ctx_.config().server.sync_max;
    int limit              = req->limit;
    if (limit <= 0)
        limit = sync_default;
    if (limit > sync_max)
        limit = sync_max;

    // 2. 从 DB 拉取 seq > last_seq 的消息
    auto messages = ctx_.dao().Message().GetAfterSeq(req->conversation_id, req->last_seq, limit);

    // 3. 构建响应（需要将 sender_id 转为 sender_uid）
    // 批量查询所有 sender（避免 N+1）
    std::vector<int64_t> sender_ids;
    sender_ids.reserve(messages.size());
    for (const auto& m : messages) {
        sender_ids.push_back(m.sender_id);
    }
    auto sender_users = ctx_.dao().User().FindByIds(sender_ids);
    std::unordered_map<int64_t, std::string> uid_cache;
    for (const auto& u : sender_users) {
        uid_cache[u.id] = u.uid;
    }
    auto resolve_uid = [&](int64_t id) -> std::string {
        auto it = uid_cache.find(id);
        return it != uid_cache.end() ? it->second : "[deleted]";
    };

    proto::SyncMsgResp resp;
    resp.code     = ec::kOk.code;
    resp.has_more = static_cast<int>(messages.size()) >= limit;
    resp.messages.reserve(messages.size());

    for (const auto& m : messages) {
        resp.messages.push_back({m.seq, resolve_uid(m.sender_id), m.content,
                                 static_cast<MsgType>(m.msg_type), m.created_at,
                                 static_cast<MsgStatus>(m.status)});
    }

    SendPacket(conn, Cmd::kSyncMsgResp, pkt.seq, 0, resp);

    NOVA_NLOG_DEBUG(kLogTag, "sync_msg: user={}, conv={}, from_seq={}, returned={}", user_id, req->conversation_id,
                    req->last_seq, messages.size());
}

void SyncService::HandleSyncUnread(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();

    if (user_id == 0) {
        SendPacket(conn, Cmd::kSyncUnreadResp, pkt.seq, 0, proto::SyncUnreadResp{ec::kNotAuthenticated.code});
        return;
    }

    // 1. 查询用户参与的所有会话
    auto memberships = ctx_.dao().Conversation().GetMembersByUser(user_id);

    // 2. 批量获取所有会话信息（1 次查询代替 N 次 FindById）
    std::vector<int64_t> conv_ids;
    conv_ids.reserve(memberships.size());
    for (const auto& m : memberships) {
        conv_ids.push_back(m.conversation_id);
    }
    auto conversations = ctx_.dao().Conversation().FindByIds(conv_ids);

    std::unordered_map<int64_t, const Conversation*> conv_map;
    conv_map.reserve(conversations.size());
    for (const auto& c : conversations) {
        conv_map[c.id] = &c;
    }

    // 3. 计算各会话未读数，收集需要预览的会话
    proto::SyncUnreadResp resp;
    resp.code = ec::kOk.code;

    // 收集有未读的会话 (conversation_id, preview_from_seq)
    std::vector<std::pair<int64_t, int64_t>> preview_convs;

    for (const auto& member : memberships) {
        auto it = conv_map.find(member.conversation_id);
        if (it == conv_map.end())
            continue;
        const auto& conv = *(it->second);

        int64_t unread = conv.max_seq - member.last_read_seq;
        if (unread < 0)
            unread = 0;
        resp.total_unread += unread;

        if (unread > 0) {
            proto::UnreadItem item;
            item.conversation_id = member.conversation_id;
            item.count           = unread;
            resp.items.push_back(std::move(item));

            auto preview_from = std::max<int64_t>(0, conv.max_seq - 3);
            preview_convs.emplace_back(member.conversation_id, preview_from);
        }
    }

    // 4. 批量获取所有有未读会话的预览消息（1 次 UNION ALL 代替 N 次查询）
    if (!preview_convs.empty()) {
        auto all_previews = ctx_.dao().Message().GetLatestByConversations(preview_convs, 3);

        // 批量查询所有 sender（避免 N+1）
        std::vector<int64_t> preview_sender_ids;
        for (const auto& m : all_previews) {
            preview_sender_ids.push_back(m.sender_id);
        }
        auto preview_users = ctx_.dao().User().FindByIds(preview_sender_ids);
        std::unordered_map<int64_t, std::string> uid_cache;
        for (const auto& u : preview_users) {
            uid_cache[u.id] = u.uid;
        }
        auto resolve_uid = [&](int64_t id) -> std::string {
            auto it = uid_cache.find(id);
            return it != uid_cache.end() ? it->second : "";
        };

        // 按 conversation_id 分组
        std::unordered_map<int64_t, std::vector<const Message*>> preview_map;
        for (const auto& m : all_previews) {
            preview_map[m.conversation_id].push_back(&m);
        }

        // 填充到对应的 UnreadItem
        for (auto& item : resp.items) {
            auto pit = preview_map.find(item.conversation_id);
            if (pit == preview_map.end())
                continue;
            for (const auto* m : pit->second) {
                item.latest_messages.push_back(
                    {m->seq, resolve_uid(m->sender_id), m->content,
                     static_cast<MsgType>(m->msg_type), m->created_at,
                     static_cast<MsgStatus>(m->status)});
            }
        }
    }

    SendPacket(conn, Cmd::kSyncUnreadResp, pkt.seq, 0, resp);

    NOVA_NLOG_DEBUG(kLogTag, "sync_unread: user={}, conversations={}, total_unread={}", user_id, memberships.size(),
                    resp.total_unread);
}

}  // namespace nova
