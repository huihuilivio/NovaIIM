#include "msg_service.h"
#include "../core/server_context.h"
#include "../core/logger.h"
#include "../dao/conversation_dao.h"
#include "../dao/message_dao.h"

#include <chrono>

namespace nova {

static constexpr const char* kLogTag = "MsgService";

template <typename T>
void MsgService::SendPacket(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const T& body) {
    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(cmd);
    pkt.seq = seq;
    pkt.uid = uid;
    pkt.body = proto::Serialize(body);
    conn->Send(pkt);
    ctx_.incr_messages_out();
}

void MsgService::HandleSendMsg(ConnectionPtr conn, Packet& pkt) {
    uint32_t seq = pkt.seq;
    int64_t sender_id = conn->user_id();
    auto uid = static_cast<uint64_t>(sender_id);

    // 未认证检查
    if (sender_id == 0) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, 0, proto::SendMsgAck{2, "not authenticated"});
        return;
    }

    // 1. 反序列化 body → SendMsgReq
    auto req = proto::Deserialize<proto::SendMsgReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{1, "invalid body"});
        return;
    }

    if (req->content.empty()) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{1, "content is empty"});
        return;
    }

    if (req->content.size() > 4096) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{5, "content too large"});
        return;
    }

    if (req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{6, "invalid conversation_id"});
        return;
    }

    // 检查发送者是否为会话成员
    if (!ctx_.dao().Conversation().IsMember(req->conversation_id, sender_id)) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{7, "not a member of this conversation"});
        return;
    }

    // 2. 生成 seq（原子递增 conversation.max_seq）
    int64_t server_seq = GenerateSeq(req->conversation_id);
    if (server_seq < 0) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{6, "conversation not found"});
        return;
    }

    // 3. 构建消息写入 DB
    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    Message msg;
    msg.conversation_id = req->conversation_id;
    msg.sender_id       = sender_id;
    msg.seq             = server_seq;
    msg.msg_type        = req->msg_type;
    msg.content         = req->content;
    msg.status          = 0;

    if (!ctx_.dao().Message().Insert(msg)) {
        NOVA_NLOG_ERROR(kLogTag, "failed to insert message conv={} sender={}",
                        req->conversation_id, sender_id);
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{100, "database error"});
        return;
    }

    // 4. 返回 SendMsgAck 给发送方
    SendPacket(conn, Cmd::kSendMsgAck, seq, uid,
               proto::SendMsgAck{0, "ok", server_seq, epoch_ms});

    // 5. 构建 PushMsg 推送给会话成员
    proto::PushMsg push{req->conversation_id, sender_id, req->content,
                        server_seq, epoch_ms, req->msg_type};

    Packet push_pkt;
    push_pkt.cmd = static_cast<uint16_t>(Cmd::kPushMsg);
    push_pkt.seq = 0;
    push_pkt.uid = 0;
    push_pkt.body = proto::Serialize(push);

    auto members = ctx_.dao().Conversation().GetMembersByConversation(req->conversation_id);
    for (const auto& member : members) {
        if (member.user_id == sender_id) {
            PushToOtherDevices(sender_id, conn->device_id(), push_pkt);
        } else {
            PushToUser(member.user_id, push_pkt);
        }
    }

    NOVA_NLOG_DEBUG(kLogTag, "msg sent: conv={}, sender={}, seq={}",
                    req->conversation_id, sender_id, server_seq);
}

void MsgService::HandleDeliverAck(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0) return;

    auto req = proto::Deserialize<proto::DeliverAckReq>(pkt.body);
    if (!req) return;

    if (req->conversation_id > 0 && req->server_seq > 0) {
        // 仅处理用户所属会话的 ACK
        if (ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
            ctx_.dao().Conversation().UpdateLastAckSeq(req->conversation_id, user_id, req->server_seq);
        }
    }
}

void MsgService::HandleReadAck(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0) return;

    auto req = proto::Deserialize<proto::ReadAckReq>(pkt.body);
    if (!req) return;

    if (req->conversation_id > 0 && req->read_up_to_seq > 0) {
        // 仅处理用户所属会话的已读回执
        if (ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
            ctx_.dao().Conversation().UpdateLastReadSeq(req->conversation_id, user_id, req->read_up_to_seq);

            SendPacket(conn, Cmd::kReadAck, pkt.seq,
                       static_cast<uint64_t>(user_id), proto::RspBase{0, {}});
        }
    }
}

int64_t MsgService::GenerateSeq(int64_t conversation_id) {
    return ctx_.dao().Conversation().IncrMaxSeq(conversation_id);
}

void MsgService::PushToUser(int64_t user_id, const Packet& pkt) {
    auto conns = ctx_.conn_manager().GetConns(user_id);
    for (auto& c : conns) {
        c->Send(pkt);
        ctx_.incr_messages_out();
    }
}

void MsgService::PushToOtherDevices(int64_t user_id, const std::string& exclude_device, const Packet& pkt) {
    auto conns = ctx_.conn_manager().GetConns(user_id);
    for (auto& c : conns) {
        if (c->device_id() != exclude_device) {
            c->Send(pkt);
            ctx_.incr_messages_out();
        }
    }
}

} // namespace nova
