#include "sync_service.h"
#include "../core/server_context.h"
#include "../core/logger.h"
#include "../dao/conversation_dao.h"
#include "../dao/message_dao.h"

#include <hv/json.hpp>

namespace nova {

static constexpr const char* kLogTag = "SyncService";
static constexpr int kDefaultSyncLimit = 20;
static constexpr int kMaxSyncLimit = 100;

void SyncService::SendReply(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const std::string& body) {
    Packet ack;
    ack.cmd = static_cast<uint16_t>(cmd);
    ack.seq = seq;
    ack.uid = uid;
    ack.body = body;
    conn->Send(ack);
    ctx_.incr_messages_out();
}

void SyncService::HandleSyncMsg(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0) {
        nlohmann::json err = {{"code", 2}, {"msg", "not authenticated"}};
        SendReply(conn, Cmd::kSyncMsgResp, pkt.seq, 0, err.dump());
        return;
    }

    // 1. 解码 body → { conversation_id, last_seq, limit }
    auto js = nlohmann::json::parse(pkt.body, nullptr, false);
    if (js.is_discarded()) {
        nlohmann::json err = {{"code", 1}, {"msg", "invalid json"}};
        SendReply(conn, Cmd::kSyncMsgResp, pkt.seq, static_cast<uint64_t>(user_id), err.dump());
        return;
    }

    int64_t conversation_id = js.value("conversation_id", 0LL);
    int64_t last_seq        = js.value("last_seq", 0LL);
    int     limit           = js.value("limit", kDefaultSyncLimit);

    if (conversation_id <= 0) {
        nlohmann::json err = {{"code", 1}, {"msg", "invalid conversation_id"}};
        SendReply(conn, Cmd::kSyncMsgResp, pkt.seq, static_cast<uint64_t>(user_id), err.dump());
        return;
    }

    // 限制拉取数量
    if (limit <= 0) limit = kDefaultSyncLimit;
    if (limit > kMaxSyncLimit) limit = kMaxSyncLimit;

    // 2. 从 DB 拉取 seq > last_seq 的消息
    auto messages = ctx_.dao().Message().GetAfterSeq(conversation_id, last_seq, limit);

    // 3. 构建响应
    nlohmann::json msg_array = nlohmann::json::array();
    for (const auto& m : messages) {
        msg_array.push_back({
            {"server_seq", m.seq},
            {"sender_id", m.sender_id},
            {"content", m.content},
            {"msg_type", m.msg_type},
            {"server_time", m.created_at},
            {"status", m.status},
        });
    }

    bool has_more = static_cast<int>(messages.size()) >= limit;

    nlohmann::json resp = {
        {"code", 0},
        {"messages", msg_array},
        {"has_more", has_more},
    };
    SendReply(conn, Cmd::kSyncMsgResp, pkt.seq, static_cast<uint64_t>(user_id), resp.dump());

    NOVA_NLOG_DEBUG(kLogTag, "sync_msg: user={}, conv={}, from_seq={}, returned={}",
                    user_id, conversation_id, last_seq, messages.size());
}

void SyncService::HandleSyncUnread(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0) {
        nlohmann::json err = {{"code", 2}, {"msg", "not authenticated"}};
        SendReply(conn, Cmd::kSyncUnreadResp, pkt.seq, 0, err.dump());
        return;
    }

    // 1. 查询用户参与的所有会话
    auto memberships = ctx_.dao().Conversation().GetMembersByUser(user_id);

    // 2. 计算各会话未读数 = conversation.max_seq - member.last_read_seq
    nlohmann::json unread_map = nlohmann::json::object();
    int64_t total_unread = 0;

    for (const auto& member : memberships) {
        auto conv = ctx_.dao().Conversation().FindById(member.conversation_id);
        if (!conv) continue;

        int64_t unread = conv->max_seq - member.last_read_seq;
        if (unread < 0) unread = 0;
        total_unread += unread;

        if (unread > 0) {
            // 拉取最近几条消息作为预览
            auto preview = ctx_.dao().Message().GetAfterSeq(
                member.conversation_id, conv->max_seq - 3, 3);

            nlohmann::json preview_arr = nlohmann::json::array();
            for (const auto& m : preview) {
                preview_arr.push_back({
                    {"server_seq", m.seq},
                    {"sender_id", m.sender_id},
                    {"content", m.content},
                    {"msg_type", m.msg_type},
                    {"server_time", m.created_at},
                });
            }

            unread_map[std::to_string(member.conversation_id)] = {
                {"count", unread},
                {"latest_messages", preview_arr},
            };
        }
    }

    // 3. 返回响应
    nlohmann::json resp = {
        {"code", 0},
        {"unread_by_conversation", unread_map},
        {"total_unread", total_unread},
    };
    SendReply(conn, Cmd::kSyncUnreadResp, pkt.seq, static_cast<uint64_t>(user_id), resp.dump());

    NOVA_NLOG_DEBUG(kLogTag, "sync_unread: user={}, conversations={}, total_unread={}",
                    user_id, memberships.size(), total_unread);
}

} // namespace nova
