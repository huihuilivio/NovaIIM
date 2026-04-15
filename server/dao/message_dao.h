#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../model/types.h"

namespace nova {

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
};

} // namespace nova
