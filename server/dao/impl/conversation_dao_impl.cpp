#include "conversation_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
int64_t ConversationDaoImplT<DbMgr>::IncrMaxSeq(int64_t conversation_id) {
    // 原子自增: 先 UPDATE max_seq = max_seq + 1，再查回新值
    auto convs = db_.DB().query_s<Conversation>("id=?", conversation_id);
    if (convs.empty()) return -1;

    convs[0].max_seq += 1;
    if (db_.DB().template update_some<&Conversation::max_seq>(convs[0]) != 1) {
        return -1;
    }
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
bool ConversationDaoImplT<DbMgr>::UpdateLastReadSeq(int64_t conversation_id, int64_t user_id, int64_t seq) {
    auto members = db_.DB().query_s<ConversationMember>(
        "conversation_id=? AND user_id=?", conversation_id, user_id);
    if (members.empty()) return false;

    // 只允许向前推进（不能回退已读位置）
    if (seq <= members[0].last_read_seq) return true;

    members[0].last_read_seq = seq;
    return db_.DB().template update_some<&ConversationMember::last_read_seq>(members[0]) == 1;
}

template <typename DbMgr>
bool ConversationDaoImplT<DbMgr>::UpdateLastAckSeq(int64_t conversation_id, int64_t user_id, int64_t seq) {
    auto members = db_.DB().query_s<ConversationMember>(
        "conversation_id=? AND user_id=?", conversation_id, user_id);
    if (members.empty()) return false;

    if (seq <= members[0].last_ack_seq) return true;

    members[0].last_ack_seq = seq;
    return db_.DB().template update_some<&ConversationMember::last_ack_seq>(members[0]) == 1;
}

// 显式实例化
template class ConversationDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class ConversationDaoImplT<MysqlDbManager>;
#endif

} // namespace nova
