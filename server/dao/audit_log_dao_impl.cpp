#include "audit_log_dao_impl.h"

namespace nova {

bool AuditLogDaoImpl::Insert(const AuditLog& log) {
    return db_.DB().insert(log) == 1;
}

AuditLogListResult AuditLogDaoImpl::List(int64_t user_id, const std::string& action,
                                          const std::string& start_time,
                                          const std::string& end_time,
                                          int page, int page_size) {
    AuditLogListResult result;
    int offset = (page - 1) * page_size;

    // 使用 SQLite 参数化恒等式：(? = 0 OR field = ?)
    // 当参数为默认值时条件恒真，否则进行实际过滤
    static constexpr auto kCountSql =
        "SELECT count(*) FROM audit_logs WHERE "
        "(? = 0 OR user_id = ?) AND "
        "(? = '' OR action = ?) AND "
        "(? = '' OR created_at >= ?) AND "
        "(? = '' OR created_at <= ?)";

    static constexpr auto kWhere =
        "(? = 0 OR user_id = ?) AND "
        "(? = '' OR action = ?) AND "
        "(? = '' OR created_at >= ?) AND "
        "(? = '' OR created_at <= ?) "
        "ORDER BY id DESC LIMIT ? OFFSET ?";

    auto cnt = db_.DB().query_s<std::tuple<int64_t>>(
        kCountSql,
        user_id, user_id,
        action, action,
        start_time, start_time,
        end_time, end_time);
    if (!cnt.empty()) result.total = std::get<0>(cnt[0]);

    result.items = db_.DB().query_s<AuditLog>(
        kWhere,
        user_id, user_id,
        action, action,
        start_time, start_time,
        end_time, end_time,
        page_size, offset);

    return result;
}

} // namespace nova
