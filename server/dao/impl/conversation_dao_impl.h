#pragma once

#include "../conversation_dao.h"
#include <array>
#include <mutex>

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class ConversationDaoImplT : public ConversationDao {
public:
    explicit ConversationDaoImplT(DbMgr& db) : db_(db) {}

    int64_t IncrMaxSeq(int64_t conversation_id) override;
    std::vector<ConversationMember> GetMembersByUser(int64_t user_id) override;
    std::vector<ConversationMember> GetMembersByConversation(int64_t conversation_id) override;
    bool UpdateLastReadSeq(int64_t conversation_id, int64_t user_id, int64_t seq) override;
    bool UpdateLastAckSeq(int64_t conversation_id, int64_t user_id, int64_t seq) override;
    bool IsMember(int64_t conversation_id, int64_t user_id) override;
    std::optional<Conversation> FindById(int64_t id) override;
    std::vector<Conversation> FindByIds(const std::vector<int64_t>& ids) override;

private:
    DbMgr& db_;

    // 分片锁：按 conversation_id 分桶，不同会话可并行生成 seq
    static constexpr size_t kSeqShardCount = 32;
    std::array<std::mutex, kSeqShardCount> seq_mutexes_;
    std::mutex& GetSeqMutex(int64_t conversation_id) {
        return seq_mutexes_[static_cast<size_t>(conversation_id) % kSeqShardCount];
    }
};

}  // namespace nova
