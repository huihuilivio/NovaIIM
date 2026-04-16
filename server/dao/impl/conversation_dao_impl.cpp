#include "conversation_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#include "../../core/defer.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
int64_t ConversationDaoImplT<DbMgr>::IncrMaxSeq(int64_t conversation_id) {
    // 分片锁：同一 conversation 串行，不同 conversation 可并行
    std::lock_guard<std::mutex> lock(GetSeqMutex(conversation_id));

    auto&& conn = db_.DB();

    // 事务包裹 UPDATE + SELECT，确保原子性（多实例场景需要数据库行锁）
    // 注意：不使用 BEGIN IMMEDIATE（SQLite 独有），BEGIN 兼容 SQLite 和 MySQL
    // 应用层分片锁已保证同一 conversation 串行，此处事务仅保证 UPDATE+SELECT 原子性
    if (!conn.execute("BEGIN")) {
        return -1;
    }

    // 事务守卫：未 COMMIT 时自动 ROLLBACK
    bool committed = false;
    NOVA_DEFER {
        if (!committed) conn.execute("ROLLBACK");
    };

    std::string sql = "UPDATE conversations SET max_seq = max_seq + 1 WHERE id = "
                      + std::to_string(conversation_id);
    if (!conn.execute(sql)) {
        return -1;
    }

    auto convs = conn.query_s<Conversation>("id=?", conversation_id);
    if (convs.empty()) {
        return -1;
    }

    int64_t new_seq = convs[0].max_seq;
    if (!conn.execute("COMMIT")) {
        return -1;
    }
    committed = true;
    return new_seq;
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
std::vector<Conversation> ConversationDaoImplT<DbMgr>::FindByIds(const std::vector<int64_t>& ids) {
    if (ids.empty()) return {};

    // 构建 IN 子句（int64_t 转字符串，无注入风险）
    std::string in_clause = "id IN (";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) in_clause += ",";
        in_clause += std::to_string(ids[i]);
    }
    in_clause += ")";

    return db_.DB().query_s<Conversation>(in_clause);
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
