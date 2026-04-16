#include "message_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
bool MessageDaoImplT<DbMgr>::Insert(const Message& msg) {
    return db_.DB().insert(msg) == 1;
}

template <typename DbMgr>
std::vector<Message> MessageDaoImplT<DbMgr>::GetAfterSeq(int64_t conversation_id, int64_t after_seq, int limit) {
    return db_.DB().query_s<Message>(
        "conversation_id=? AND seq>? ORDER BY seq ASC LIMIT ?",
        conversation_id, after_seq, limit);
}

template <typename DbMgr>
bool MessageDaoImplT<DbMgr>::UpdateStatus(int64_t msg_id, int8_t status) {
    auto res = db_.DB().query_s<Message>("id=?", msg_id);
    if (res.empty()) return false;
    res[0].status = status;
    return db_.DB().update_some<&Message::status>(res[0]) == 1;
}

template <typename DbMgr>
MessageListResult MessageDaoImplT<DbMgr>::ListMessages(int64_t conversation_id,
                                                const std::string& start_time,
                                                const std::string& end_time,
                                                int page, int page_size) {
    MessageListResult result;
    int offset = (page - 1) * page_size;

    static constexpr auto kCountSql =
        "SELECT count(*) FROM messages WHERE "
        "(? = 0 OR conversation_id = ?) AND "
        "(? = '' OR created_at >= ?) AND "
        "(? = '' OR created_at <= ?)";

    static constexpr auto kWhere =
        "(? = 0 OR conversation_id = ?) AND "
        "(? = '' OR created_at >= ?) AND "
        "(? = '' OR created_at <= ?) "
        "ORDER BY id DESC LIMIT ? OFFSET ?";

    auto cnt = db_.DB().query_s<std::tuple<int64_t>>(
        kCountSql,
        conversation_id, conversation_id,
        start_time, start_time,
        end_time, end_time);
    if (!cnt.empty()) result.total = std::get<0>(cnt[0]);

    result.items = db_.DB().query_s<Message>(
        kWhere,
        conversation_id, conversation_id,
        start_time, start_time,
        end_time, end_time,
        page_size, offset);

    return result;
}

template <typename DbMgr>
std::optional<Message> MessageDaoImplT<DbMgr>::FindById(int64_t id) {
    auto res = db_.DB().query_s<Message>("id=?", id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::vector<Message> MessageDaoImplT<DbMgr>::GetLatestByConversations(
        const std::vector<std::pair<int64_t, int64_t>>& conv_from_seqs,
        int limit_per_conv) {
    if (conv_from_seqs.empty()) return {};

    // 分批查询：每批最多 100 个会话，防止 UNION ALL SQL 过长
    // SQLite 默认限制 ~1MB SQL, MySQL max_allowed_packet 也有限
    static constexpr size_t kBatchSize = 100;

    std::vector<Message> all_results;
    for (size_t offset = 0; offset < conv_from_seqs.size(); offset += kBatchSize) {
        size_t end = std::min(offset + kBatchSize, conv_from_seqs.size());

        // 构建 UNION ALL 查询：每个会话取最新 limit_per_conv 条
        // 所有参数都是 int64_t，无注入风险
        std::string sql = "SELECT * FROM (";
        for (size_t i = offset; i < end; ++i) {
            if (i > offset) sql += " UNION ALL ";
            sql += "SELECT * FROM messages WHERE conversation_id = "
                 + std::to_string(conv_from_seqs[i].first)
                 + " AND seq > "
                 + std::to_string(conv_from_seqs[i].second)
                 + " ORDER BY seq DESC LIMIT "
                 + std::to_string(limit_per_conv);
        }
        sql += ")";

        auto batch = db_.DB().template query_s<Message>(sql);
        all_results.insert(all_results.end(),
                           std::make_move_iterator(batch.begin()),
                           std::make_move_iterator(batch.end()));
    }

    return all_results;
}

// 显式实例化
template class MessageDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class MessageDaoImplT<MysqlDbManager>;
#endif

} // namespace nova
