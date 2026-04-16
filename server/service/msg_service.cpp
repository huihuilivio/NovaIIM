#include "msg_service.h"
#include "errors/msg_errors.h"
#include "../core/logger.h"
#include "../dao/conversation_dao.h"
#include "../dao/message_dao.h"

#include <chrono>

namespace nova {

using namespace errc;

static constexpr const char* kLogTag = "MsgService";

void MsgService::HandleSendMsg(ConnectionPtr conn, Packet& pkt) {
    uint32_t seq = pkt.seq;
    int64_t sender_id = conn->user_id();
    auto uid = static_cast<uint64_t>(sender_id);

    // 未认证检查
    if (sender_id == 0) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, 0, proto::SendMsgAck{kNotAuthenticated.code, kNotAuthenticated.msg});
        return;
    }

    // 1. 反序列化 body → SendMsgReq
    auto req = proto::Deserialize<proto::SendMsgReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{kInvalidBody.code, kInvalidBody.msg});
        return;
    }

    if (req->content.empty()) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{msg::kContentEmpty.code, msg::kContentEmpty.msg});
        return;
    }

    if (req->content.size() > 4096) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{msg::kContentTooLarge.code, msg::kContentTooLarge.msg});
        return;
    }

    if (req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{msg::kInvalidConversation.code, msg::kInvalidConversation.msg});
        return;
    }

    // 幂等去重：如果 client_msg_id 已处理过，直接返回缓存的 Ack
    if (!req->client_msg_id.empty()) {
        std::lock_guard<std::mutex> lock(dedup_mutex_);
        auto it = dedup_cache_.find(req->client_msg_id);
        if (it != dedup_cache_.end()) {
            SendPacket(conn, Cmd::kSendMsgAck, seq, uid, it->second);
            return;
        }
    }

    // 检查发送者是否为会话成员
    if (!ctx_.dao().Conversation().IsMember(req->conversation_id, sender_id)) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{msg::kNotMember.code, msg::kNotMember.msg});
        return;
    }

    // 2. 生成 seq（原子递增 conversation.max_seq）
    int64_t server_seq = GenerateSeq(req->conversation_id);
    if (server_seq < 0) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{msg::kConversationNotFound.code, msg::kConversationNotFound.msg});
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
    msg.client_msg_id   = req->client_msg_id;
    msg.status          = 0;

    if (!ctx_.dao().Message().Insert(msg)) {
        NOVA_NLOG_ERROR(kLogTag, "failed to insert message conv={} sender={}",
                        req->conversation_id, sender_id);
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{kDatabaseError.code, kDatabaseError.msg});
        return;
    }

    // 4. 构建 Ack 并缓存（幂等去重）
    proto::SendMsgAck ack{kOk.code, kOk.msg, server_seq, epoch_ms};

    if (!req->client_msg_id.empty()) {
        std::lock_guard<std::mutex> lock(dedup_mutex_);
        if (dedup_cache_.size() >= kMaxDedupCacheSize) {
            dedup_cache_.clear();
        }
        dedup_cache_[req->client_msg_id] = ack;
    }

    // 5. 返回 SendMsgAck 给发送方
    SendPacket(conn, Cmd::kSendMsgAck, seq, uid, ack);

    // 6. 构建 PushMsg 并一次性编码，广播时避免重复编码
    proto::PushMsg push{req->conversation_id, sender_id, req->content,
                        server_seq, epoch_ms, req->msg_type};

    Packet push_pkt;
    push_pkt.cmd = static_cast<uint16_t>(Cmd::kPushMsg);
    push_pkt.seq = 0;
    push_pkt.uid = 0;
    push_pkt.body = proto::Serialize(push);

    std::string encoded = push_pkt.Encode();

    BroadcastEncoded(sender_id, conn->device_id(), req->conversation_id, encoded);

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
                       static_cast<uint64_t>(user_id), proto::RspBase{kOk.code, {}});
        }
    }
}

int64_t MsgService::GenerateSeq(int64_t conversation_id) {
    return ctx_.dao().Conversation().IncrMaxSeq(conversation_id);
}

void MsgService::BroadcastEncoded(int64_t sender_id, const std::string& exclude_device,
                                  int64_t conversation_id, const std::string& encoded) {
    auto members = ctx_.dao().Conversation().GetMembersByConversation(conversation_id);
    for (const auto& member : members) {
        auto conns = ctx_.conn_manager().GetConns(member.user_id);
        for (auto& c : conns) {
            // 发送方排除当前设备，其他成员全推
            if (member.user_id == sender_id && c->device_id() == exclude_device) {
                continue;
            }
            c->SendEncoded(encoded);
            ctx_.incr_messages_out();
        }
    }
}

} // namespace nova
