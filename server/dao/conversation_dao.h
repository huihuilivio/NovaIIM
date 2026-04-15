#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../model/types.h"

namespace nova {

// 会话 DAO
class ConversationDao {
public:
    virtual ~ConversationDao() = default;

    // 原子递增 max_seq 并返回新值（seq 生成核心）
    virtual int64_t IncrMaxSeq(int64_t conversation_id) = 0;

    // 获取用户参与的所有会话成员关系
    virtual std::vector<ConversationMember> GetMembersByUser(int64_t user_id) = 0;

    // 更新 last_read_seq（已读回执）
    virtual bool UpdateLastReadSeq(int64_t conversation_id, int64_t user_id, int64_t seq) = 0;

    // 更新 last_ack_seq（投递确认）
    virtual bool UpdateLastAckSeq(int64_t conversation_id, int64_t user_id, int64_t seq) = 0;
};

} // namespace nova
