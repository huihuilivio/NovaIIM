#pragma once

#include "service_base.h"
#include <chrono>
#include <list>
#include <mutex>
#include <unordered_map>

namespace nova {

// 消息服务（对应架构文档 4.4 MsgService）
// 职责：seq 生成、写 DB、推送消息、ACK 处理、消息幂等去重
class MsgService : public ServiceBase {
public:
    explicit MsgService(ServerContext& ctx);

    void HandleSendMsg(ConnectionPtr conn, Packet& pkt);
    void HandleRecallMsg(ConnectionPtr conn, Packet& pkt);
    void HandleDeliverAck(ConnectionPtr conn, Packet& pkt);
    void HandleReadAck(ConnectionPtr conn, Packet& pkt);

private:
    int64_t GenerateSeq(int64_t conversation_id);
    void BroadcastEncoded(int64_t sender_id, const std::string& exclude_device, int64_t conversation_id,
                          const std::string& encoded);

    // ---- 消息幂等去重缓存（LRU：淘汰最旧条目，避免全量清空） ----
    size_t max_dedup_cache_size_;
    size_t max_content_size_;
    int recall_timeout_secs_;
    static constexpr std::chrono::seconds kInflightTimeout{30};  // in-flight 超时
    std::mutex dedup_mutex_; // todo: 细化锁粒度（读写分离）以提升并发性能, 分片锁等
    // LRU 列表：front = 最旧，back = 最新
    using DedupEntry = std::pair<std::string, proto::SendMsgAck>;  // key, value
    std::list<DedupEntry> dedup_order_;
    std::unordered_map<std::string, std::list<DedupEntry>::iterator> dedup_index_;

    // 正在处理中的 client_msg_id → 标记时间（防止 TOCTOU 竞态）
    // 超过 kInflightTimeout 的条目视为过期，允许重入
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> in_flight_;

    // 查找（调用方需持锁）
    proto::SendMsgAck* DedupFind(const std::string& key);
    // 插入或更新（调用方需持锁）
    void DedupInsert(const std::string& key, const proto::SendMsgAck& ack);
    // 尝试标记 in-flight，返回 true 表示成功（调用方需持锁）
    // 超时的旧条目会被自动覆盖
    bool TryMarkInflight(const std::string& key);
    // 移除 in-flight 标记（调用方需持锁）
    void DedupRemoveInflight(const std::string& key);
    // 便捷方法：非空 key 时加锁并移除 in-flight 标记
    void DedupRemoveInflightIfNeeded(const std::string& key);
};

}  // namespace nova
