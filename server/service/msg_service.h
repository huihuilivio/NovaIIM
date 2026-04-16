#pragma once

#include "service_base.h"
#include <mutex>
#include <unordered_map>

namespace nova {

// 消息服务（对应架构文档 4.4 MsgService）
// 职责：seq 生成、写 DB、推送消息、ACK 处理、消息幂等去重
class MsgService : public ServiceBase {
public:
    explicit MsgService(ServerContext& ctx) : ServiceBase(ctx) {}

    void HandleSendMsg(ConnectionPtr conn, Packet& pkt);
    void HandleDeliverAck(ConnectionPtr conn, Packet& pkt);
    void HandleReadAck(ConnectionPtr conn, Packet& pkt);

private:
    int64_t GenerateSeq(int64_t conversation_id);
    void BroadcastEncoded(int64_t sender_id, const std::string& exclude_device,
                          int64_t conversation_id, const std::string& encoded);

    // 消息幂等去重缓存（client_msg_id → SendMsgAck）
    static constexpr size_t kMaxDedupCacheSize = 10000;
    std::mutex dedup_mutex_;
    std::unordered_map<std::string, proto::SendMsgAck> dedup_cache_;
};

} // namespace nova
