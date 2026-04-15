#include "message_dao_impl.h"

namespace nova {

bool MessageDaoImpl::Insert(const Message& msg) {
    return db_.DB().insert(msg) == 1;
}

std::vector<Message> MessageDaoImpl::GetAfterSeq(int64_t conversation_id, int64_t after_seq, int limit) {
    return db_.DB().query_s<Message>(
        "conversation_id=? AND seq>? ORDER BY seq ASC LIMIT ?",
        conversation_id, after_seq, limit);
}

bool MessageDaoImpl::UpdateStatus(int64_t msg_id, int8_t status) {
    auto res = db_.DB().query_s<Message>("id=?", msg_id);
    if (res.empty()) return false;
    res[0].status = status;
    return db_.DB().update_some<&Message::status>(res[0]) == 1;
}

MessageListResult MessageDaoImpl::ListMessages(int64_t conversation_id,
                                                const std::string& start_time,
                                                const std::string& end_time,
                                                int page, int page_size) {
    MessageListResult result;
    int offset = (page - 1) * page_size;

    // 使用 SQLite 参数化恒等式：(? = 0 OR field = ?)
    // 当参数为默认值时条件恒真，否则进行实际过滤，避免分支爆炸
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

std::optional<Message> MessageDaoImpl::FindById(int64_t id) {
    auto res = db_.DB().query_s<Message>("id=?", id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

} // namespace nova
