#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "../model/types.h"

namespace nova {

struct MessageListResult {
    std::vector<Message> items;
    int64_t total = 0;
};

// 消息 DAO（对应架构文档 DB Layer）
class MessageDao {
public:
    virtual ~MessageDao() = default;

    // 插入消息
    virtual bool Insert(const Message& msg) = 0;

    // 根据会话 + seq 范围拉取消息（离线同步）
    virtual std::vector<Message> GetAfterSeq(int64_t conversation_id, int64_t after_seq, int limit = 100) = 0;

    // 更新消息状态（撤回/删除）
    virtual bool UpdateStatus(int64_t msg_id, int8_t status) = 0;

    // 按会话 + 时间范围分页查询（admin 用）
    virtual MessageListResult ListMessages(int64_t conversation_id, const std::string& start_time,
                                           const std::string& end_time, int page, int page_size) = 0;

    // 根据 ID 查找
    virtual std::optional<Message> FindById(int64_t id) = 0;

    // 批量获取多个会话的最近 N 条消息（用于未读预览，避免 N+1 查询）
    // 返回所有匹配的消息，调用方按 conversation_id 分组
    virtual std::vector<Message>
    GetLatestByConversations(const std::vector<std::pair<int64_t, int64_t>>& conv_from_seqs, int limit_per_conv) = 0;
};

}  // namespace nova
