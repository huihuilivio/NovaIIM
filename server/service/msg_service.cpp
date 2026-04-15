#include "msg_service.h"
#include "../net/conn_manager.h"
#include "../core/server_context.h"
#include "../core/logger.h"
#include "../dao/conversation_dao.h"
#include "../dao/message_dao.h"

#include <hv/json.hpp>

#include <chrono>

namespace nova {

static constexpr const char* kLogTag = "MsgService";

void MsgService::SendReply(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const std::string& body) {
    Packet ack;
    ack.cmd = static_cast<uint16_t>(cmd);
    ack.seq = seq;
    ack.uid = uid;
    ack.body = body;
    conn->Send(ack);
    ctx_.incr_messages_out();
}

void MsgService::HandleSendMsg(ConnectionPtr conn, Packet& pkt) {
    uint32_t seq = pkt.seq;
    int64_t sender_id = conn->user_id();

    // 未认证检查
    if (sender_id == 0) {
        nlohmann::json err = {{"code", 2}, {"msg", "not authenticated"}};
        SendReply(conn, Cmd::kSendMsgAck, seq, 0, err.dump());
        return;
    }

    // 1. 解码 body → JSON
    auto js = nlohmann::json::parse(pkt.body, nullptr, false);
    if (js.is_discarded()) {
        nlohmann::json err = {{"code", 1}, {"msg", "invalid json"}};
        SendReply(conn, Cmd::kSendMsgAck, seq, static_cast<uint64_t>(sender_id), err.dump());
        return;
    }

    int64_t conversation_id = js.value("conversation_id", 0LL);
    std::string content     = js.value("content", "");
    int msg_type            = js.value("msg_type", 1);

    if (content.empty()) {
        nlohmann::json err = {{"code", 1}, {"msg", "content is empty"}};
        SendReply(conn, Cmd::kSendMsgAck, seq, static_cast<uint64_t>(sender_id), err.dump());
        return;
    }

    if (content.size() > 4096) {
        nlohmann::json err = {{"code", 5}, {"msg", "content too large"}};
        SendReply(conn, Cmd::kSendMsgAck, seq, static_cast<uint64_t>(sender_id), err.dump());
        return;
    }

    if (conversation_id <= 0) {
        nlohmann::json err = {{"code", 6}, {"msg", "invalid conversation_id"}};
        SendReply(conn, Cmd::kSendMsgAck, seq, static_cast<uint64_t>(sender_id), err.dump());
        return;
    }

    // 2. 生成 seq（原子递增 conversation.max_seq）
    int64_t server_seq = GenerateSeq(conversation_id);
    if (server_seq < 0) {
        nlohmann::json err = {{"code", 6}, {"msg", "conversation not found"}};
        SendReply(conn, Cmd::kSendMsgAck, seq, static_cast<uint64_t>(sender_id), err.dump());
        return;
    }

    // 3. 构建消息写入 DB
    auto now = std::chrono::system_clock::now();
    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    Message msg;
    msg.conversation_id = conversation_id;
    msg.sender_id       = sender_id;
    msg.seq             = server_seq;
    msg.msg_type        = msg_type;
    msg.content         = content;
    msg.status          = 0;  // 0=正常

    if (!ctx_.dao().Message().Insert(msg)) {
        NOVA_NLOG_ERROR(kLogTag, "failed to insert message for conv={}, sender={}", conversation_id, sender_id);
        nlohmann::json err = {{"code", 100}, {"msg", "database error"}};
        SendReply(conn, Cmd::kSendMsgAck, seq, static_cast<uint64_t>(sender_id), err.dump());
        return;
    }

    // 4. 返回 SendMsgAck 给发送方
    nlohmann::json ack_body = {
        {"code", 0},
        {"msg", "ok"},
        {"server_seq", server_seq},
        {"server_time", epoch_ms},
    };
    SendReply(conn, Cmd::kSendMsgAck, seq, static_cast<uint64_t>(sender_id), ack_body.dump());

    // 5. 构建 PushMsg 包，推送给会话中的其他成员
    nlohmann::json push_body = {
        {"conversation_id", conversation_id},
        {"sender_id", sender_id},
        {"content", content},
        {"server_seq", server_seq},
        {"server_time", epoch_ms},
        {"msg_type", msg_type},
    };

    Packet push_pkt;
    push_pkt.cmd = static_cast<uint16_t>(Cmd::kPushMsg);
    push_pkt.seq = 0;
    push_pkt.uid = 0;
    push_pkt.body = push_body.dump();

    // 获取会话所有成员，推送给在线的接收方
    auto members = ctx_.dao().Conversation().GetMembersByConversation(conversation_id);
    for (const auto& member : members) {
        if (member.user_id == sender_id) {
            // 发送者的其他设备也需要同步
            PushToOtherDevices(sender_id, conn->device_id(), push_pkt);
        } else {
            // 其他成员的所有设备
            PushToUser(member.user_id, push_pkt);
        }
    }

    NOVA_NLOG_DEBUG(kLogTag, "msg sent: conv={}, sender={}, seq={}", conversation_id, sender_id, server_seq);
}

void MsgService::HandleDeliverAck(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0) return;

    auto js = nlohmann::json::parse(pkt.body, nullptr, false);
    if (js.is_discarded()) return;

    int64_t server_seq      = js.value("server_seq", 0LL);
    int64_t conversation_id = js.value("conversation_id", 0LL);

    if (conversation_id > 0 && server_seq > 0) {
        ctx_.dao().Conversation().UpdateLastAckSeq(conversation_id, user_id, server_seq);
    }
}

void MsgService::HandleReadAck(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0) return;

    auto js = nlohmann::json::parse(pkt.body, nullptr, false);
    if (js.is_discarded()) return;

    int64_t conversation_id = js.value("conversation_id", 0LL);
    int64_t read_up_to_seq  = js.value("read_up_to_seq", 0LL);

    if (conversation_id > 0 && read_up_to_seq > 0) {
        ctx_.dao().Conversation().UpdateLastReadSeq(conversation_id, user_id, read_up_to_seq);

        nlohmann::json resp = {{"code", 0}};
        SendReply(conn, Cmd::kReadAck, pkt.seq, static_cast<uint64_t>(user_id), resp.dump());
    }
}

int64_t MsgService::GenerateSeq(int64_t conversation_id) {
    return ctx_.dao().Conversation().IncrMaxSeq(conversation_id);
}

void MsgService::PushToUser(int64_t user_id, const Packet& pkt) {
    auto conns = ConnManager::Instance().GetConns(user_id);
    for (auto& c : conns) {
        c->Send(pkt);
        ctx_.incr_messages_out();
    }
}

void MsgService::PushToOtherDevices(int64_t user_id, const std::string& exclude_device, const Packet& pkt) {
    auto conns = ConnManager::Instance().GetConns(user_id);
    for (auto& c : conns) {
        if (c->device_id() != exclude_device) {
            c->Send(pkt);
            ctx_.incr_messages_out();
        }
    }
}

} // namespace nova
