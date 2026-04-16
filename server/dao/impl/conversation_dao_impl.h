#pragma once

#include "../conversation_dao.h"
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

private:
    DbMgr& db_;
    std::mutex seq_mutex_;  // serializes IncrMaxSeq to prevent duplicate seq
};

} // namespace nova
