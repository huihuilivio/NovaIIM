#include "conversation_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
int64_t ConversationDaoImplT<DbMgr>::IncrMaxSeq(int64_t conversation_id) {
    // 用 mutex 序列化同一进程内的并发 IncrMaxSeq 调用，
    // 再用 SQL 原子 UPDATE 保证数据库层面的正确性
    std::lock_guard<std::mutex> lock(seq_mutex_);

    auto&& conn = db_.DB();  // 单次获取，MySQL 下复用同一池连接

    std::string sql = "UPDATE conversations SET max_seq = max_seq + 1 WHERE id = "
                      + std::to_string(conversation_id);
    if (!conn.execute(sql)) {
        return -1;
    }

    auto convs = conn.query_s<Conversation>("id=?", conversation_id);
    if (convs.empty()) return -1;
    return convs[0].max_seq;
}

template <typename DbMgr>
std::vector<ConversationMember> ConversationDaoImplT<DbMgr>::GetMembersByUser(int64_t user_id) {
    return db_.DB().query_s<ConversationMember>("user_id=?", user_id);
}

template <typename DbMgr>
std::vector<ConversationMember> ConversationDaoImplT<DbMgr>::GetMembersByConversation(int64_t conversation_id) {
    return db_.DB().query_s<ConversationMember>("conversation_id=?", conversation_id);
}

template <typename DbMgr>
std::optional<Conversation> ConversationDaoImplT<DbMgr>::FindById(int64_t id) {
    auto res = db_.DB().query_s<Conversation>("id=?", id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

template <typename DbMgr>
bool ConversationDaoImplT<DbMgr>::IsMember(int64_t conversation_id, int64_t user_id) {
    auto res = db_.DB().query_s<ConversationMember>(
        "conversation_id=? AND user_id=?", conversation_id, user_id);
    return !res.empty();
}

template <typename DbMgr>
bool ConversationDaoImplT<DbMgr>::UpdateLastReadSeq(int64_t conversation_id, int64_t user_id, int64_t seq) {
    // 原子更新：只允许向前推进（WHERE last_read_seq < ? 防止回退）
    std::string sql = "UPDATE conversation_members SET last_read_seq = "
                      + std::to_string(seq)
                      + " WHERE conversation_id = " + std::to_string(conversation_id)
                      + " AND user_id = " + std::to_string(user_id)
                      + " AND last_read_seq < " + std::to_string(seq);
    return db_.DB().execute(sql);
}

template <typename DbMgr>
bool ConversationDaoImplT<DbMgr>::UpdateLastAckSeq(int64_t conversation_id, int64_t user_id, int64_t seq) {
    // 原子更新：只允许向前推进（WHERE last_ack_seq < ? 防止回退）
    std::string sql = "UPDATE conversation_members SET last_ack_seq = "
                      + std::to_string(seq)
                      + " WHERE conversation_id = " + std::to_string(conversation_id)
                      + " AND user_id = " + std::to_string(user_id)
                      + " AND last_ack_seq < " + std::to_string(seq);
    return db_.DB().execute(sql);
}

// 显式实例化
template class ConversationDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class ConversationDaoImplT<MysqlDbManager>;
#endif

} // namespace nova
