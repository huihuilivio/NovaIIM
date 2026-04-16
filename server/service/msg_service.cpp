#include "msg_service.h"
#include "errors/msg_errors.h"
#include "../core/logger.h"
#include "../dao/conversation_dao.h"
#include "../dao/message_dao.h"

#include <chrono>

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "MsgService";

MsgService::MsgService(ServerContext& ctx)
    : ServiceBase(ctx),
      max_dedup_cache_size_(static_cast<size_t>(ctx.config().server.dedup_cache_size)),
      max_content_size_(static_cast<size_t>(ctx.config().server.max_content_size)) {}

// ---- LRU dedup 缓存实现 ----

proto::SendMsgAck* MsgService::DedupFind(const std::string& key) {
    auto it = dedup_index_.find(key);
    if (it == dedup_index_.end())
        return nullptr;
    // 移到最新（back）
    dedup_order_.splice(dedup_order_.end(), dedup_order_, it->second);
    return &it->second->second;
}

void MsgService::DedupInsert(const std::string& key, const proto::SendMsgAck& ack) {
    auto it = dedup_index_.find(key);
    if (it != dedup_index_.end()) {
        it->second->second = ack;
        dedup_order_.splice(dedup_order_.end(), dedup_order_, it->second);
        return;
    }
    // 淘汰最旧条目（front）直到低于阈值
    while (dedup_index_.size() >= max_dedup_cache_size_) {
        auto& oldest = dedup_order_.front();
        dedup_index_.erase(oldest.first);
        dedup_order_.pop_front();
    }
    dedup_order_.emplace_back(key, ack);
    dedup_index_[key] = std::prev(dedup_order_.end());
}

void MsgService::DedupRemoveInflight(const std::string& key) {
    in_flight_.erase(key);
}

bool MsgService::TryMarkInflight(const std::string& key) {
    auto now            = std::chrono::steady_clock::now();
    auto [it, inserted] = in_flight_.emplace(key, now);
    if (inserted)
        return true;
    // 已存在：检查是否超时（防止卡死的线程永久阻塞后续请求）
    if (now - it->second >= kInflightTimeout) {
        it->second = now;  // 续期，允许本线程接管
        return true;
    }
    return false;  // 仍在有效期内，视为重复
}

// 便捷方法：非空 key 时在锁内移除 in-flight 标记
void MsgService::DedupRemoveInflightIfNeeded(const std::string& key) {
    if (!key.empty()) {
        std::lock_guard<std::mutex> lock(dedup_mutex_);
        in_flight_.erase(key);
    }
}

// ---- 业务处理 ----

void MsgService::HandleSendMsg(ConnectionPtr conn, Packet& pkt) {
    auto session      = ctx_.dao().Session();
    uint32_t seq      = pkt.seq;
    int64_t sender_id = conn->user_id();
    auto uid          = static_cast<uint64_t>(sender_id);

    // 未认证检查
    if (sender_id == 0) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, 0,
                   proto::SendMsgAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    // 1. 反序列化 body → SendMsgReq
    auto req = proto::Deserialize<proto::SendMsgReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid, proto::SendMsgAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->content.empty()) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid,
                   proto::SendMsgAck{ec::msg::kContentEmpty.code, ec::msg::kContentEmpty.msg});
        return;
    }

    if (req->content.size() > max_content_size_) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid,
                   proto::SendMsgAck{ec::msg::kContentTooLarge.code, ec::msg::kContentTooLarge.msg});
        return;
    }

    if (req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid,
                   proto::SendMsgAck{ec::msg::kInvalidConversation.code, ec::msg::kInvalidConversation.msg});
        return;
    }

    // 幂等去重：如果 client_msg_id 已处理过，直接返回缓存的 Ack
    // 若正在处理中（in-flight），视为重复提交并丢弃，客户端会超时重试并命中缓存
    if (!req->client_msg_id.empty()) {
        std::lock_guard<std::mutex> lock(dedup_mutex_);
        if (auto* cached = DedupFind(req->client_msg_id)) {
            SendPacket(conn, Cmd::kSendMsgAck, seq, uid, *cached);
            return;
        }
        // 防止 TOCTOU 竞态：查找未命中时立即标记 in-flight，其他线程会看到此标记
        // 超过 kInflightTimeout 的旧标记会被自动覆盖，防止永久阻塞
        if (!TryMarkInflight(req->client_msg_id)) {
            // 已有线程正在处理同一 client_msg_id，返回繁忙提示而非静默丢弃
            SendPacket(conn, Cmd::kSendMsgAck, seq, uid,
                       proto::SendMsgAck{ec::kServerBusy.code, ec::kServerBusy.msg});
            return;
        }
    }

    // 检查发送者是否为会话成员
    if (!ctx_.dao().Conversation().IsMember(req->conversation_id, sender_id)) {
        DedupRemoveInflightIfNeeded(req->client_msg_id);
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid,
                   proto::SendMsgAck{ec::msg::kNotMember.code, ec::msg::kNotMember.msg});
        return;
    }

    // 2. 生成 seq（原子递增 conversation.max_seq）
    int64_t server_seq = GenerateSeq(req->conversation_id);
    if (server_seq < 0) {
        DedupRemoveInflightIfNeeded(req->client_msg_id);
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid,
                   proto::SendMsgAck{ec::msg::kConversationNotFound.code, ec::msg::kConversationNotFound.msg});
        return;
    }

    // 3. 构建消息写入 DB
    auto now_tp   = std::chrono::system_clock::now();
    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_tp.time_since_epoch()).count();

    // 生成 ISO-8601 时间戳，确保 Ack 返回的 epoch_ms 与 DB 记录一致
    auto now_t = std::chrono::system_clock::to_time_t(now_tp);
    char time_buf[32];
    struct tm tm_buf {};
#ifdef _MSC_VER
    gmtime_s(&tm_buf, &now_t);
#else
    gmtime_r(&now_t, &tm_buf);
#endif
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_buf);

    Message msg;
    msg.conversation_id = req->conversation_id;
    msg.sender_id       = sender_id;
    msg.seq             = server_seq;
    msg.msg_type        = req->msg_type;
    msg.content         = req->content;
    msg.client_msg_id   = req->client_msg_id;
    msg.status          = static_cast<int>(MsgStatus::Normal);
    msg.created_at      = time_buf;

    if (!ctx_.dao().Message().Insert(msg)) {
        NOVA_NLOG_ERROR(kLogTag, "failed to insert message conv={} sender={}", req->conversation_id, sender_id);
        DedupRemoveInflightIfNeeded(req->client_msg_id);
        SendPacket(conn, Cmd::kSendMsgAck, seq, uid,
                   proto::SendMsgAck{ec::kDatabaseError.code, ec::kDatabaseError.msg});
        return;
    }

    // 4. 构建 Ack 并缓存（幂等去重 LRU），同时移除 in-flight 标记
    proto::SendMsgAck ack{ec::kOk.code, ec::kOk.msg, server_seq, epoch_ms};

    if (!req->client_msg_id.empty()) {
        std::lock_guard<std::mutex> lock(dedup_mutex_);
        in_flight_.erase(req->client_msg_id);
        DedupInsert(req->client_msg_id, ack);
    }

    // 5. 返回 SendMsgAck 给发送方
    SendPacket(conn, Cmd::kSendMsgAck, seq, uid, ack);

    // 6. 构建 PushMsg 并一次性编码，广播时避免重复编码
    proto::PushMsg push{req->conversation_id, sender_id, req->content, server_seq, epoch_ms, req->msg_type};

    Packet push_pkt;
    push_pkt.cmd  = static_cast<uint16_t>(Cmd::kPushMsg);
    push_pkt.seq  = 0;
    push_pkt.uid  = 0;
    push_pkt.body = proto::Serialize(push);

    std::string encoded = push_pkt.Encode();

    BroadcastEncoded(sender_id, conn->device_id(), req->conversation_id, encoded);

    NOVA_NLOG_DEBUG(kLogTag, "msg sent: conv={}, sender={}, seq={}", req->conversation_id, sender_id, server_seq);
}

void MsgService::HandleDeliverAck(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0)
        return;

    auto session = ctx_.dao().Session();
    auto req     = proto::Deserialize<proto::DeliverAckReq>(pkt.body);
    if (!req)
        return;

    if (req->conversation_id > 0 && req->server_seq > 0) {
        // 仅处理用户所属会话的 ACK
        if (ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
            ctx_.dao().Conversation().UpdateLastAckSeq(req->conversation_id, user_id, req->server_seq);
        }
    }
}

void MsgService::HandleReadAck(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0)
        return;

    auto session = ctx_.dao().Session();
    auto req     = proto::Deserialize<proto::ReadAckReq>(pkt.body);
    if (!req)
        return;

    if (req->conversation_id > 0 && req->read_up_to_seq > 0) {
        // 仅处理用户所属会话的已读回执
        if (ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
            ctx_.dao().Conversation().UpdateLastReadSeq(req->conversation_id, user_id, req->read_up_to_seq);

            SendPacket(conn, Cmd::kReadAck, pkt.seq, static_cast<uint64_t>(user_id), proto::RspBase{ec::kOk.code, {}});
        }
    }
}

int64_t MsgService::GenerateSeq(int64_t conversation_id) {
    return ctx_.dao().Conversation().IncrMaxSeq(conversation_id);
}

// 注意：必须在持有 DaoScopedConn (Session) 的上下文中调用，内部会访问 ConversationDao
void MsgService::BroadcastEncoded(int64_t sender_id, const std::string& exclude_device, int64_t conversation_id,
                                  const std::string& encoded) {
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

}  // namespace nova
