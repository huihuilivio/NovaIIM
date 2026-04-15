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

    // 根据过滤条件组合不同的参数化查询路径
    // ormpp query_s 使用 ? 占位符 + 参数绑定，防 SQL 注入
    if (user_id > 0 && !action.empty()) {
        std::string cond = "user_id=? AND action=?";
        if (!start_time.empty()) cond += " AND created_at>=?";
        if (!end_time.empty())   cond += " AND created_at<=?";

        if (!start_time.empty() && !end_time.empty()) {
            auto cnt = db_.DB().query_s<std::tuple<int64_t>>("SELECT count(*) FROM audit_logs WHERE " + cond, user_id, action, start_time, end_time);
            if (!cnt.empty()) result.total = std::get<0>(cnt[0]);
            result.items = db_.DB().query_s<AuditLog>(cond + " ORDER BY id DESC LIMIT ? OFFSET ?", user_id, action, start_time, end_time, page_size, offset);
        } else if (!start_time.empty()) {
            auto cnt = db_.DB().query_s<std::tuple<int64_t>>("SELECT count(*) FROM audit_logs WHERE " + cond, user_id, action, start_time);
            if (!cnt.empty()) result.total = std::get<0>(cnt[0]);
            result.items = db_.DB().query_s<AuditLog>(cond + " ORDER BY id DESC LIMIT ? OFFSET ?", user_id, action, start_time, page_size, offset);
        } else if (!end_time.empty()) {
            auto cnt = db_.DB().query_s<std::tuple<int64_t>>("SELECT count(*) FROM audit_logs WHERE " + cond, user_id, action, end_time);
            if (!cnt.empty()) result.total = std::get<0>(cnt[0]);
            result.items = db_.DB().query_s<AuditLog>(cond + " ORDER BY id DESC LIMIT ? OFFSET ?", user_id, action, end_time, page_size, offset);
        } else {
            auto cnt = db_.DB().query_s<std::tuple<int64_t>>("SELECT count(*) FROM audit_logs WHERE " + cond, user_id, action);
            if (!cnt.empty()) result.total = std::get<0>(cnt[0]);
            result.items = db_.DB().query_s<AuditLog>(cond + " ORDER BY id DESC LIMIT ? OFFSET ?", user_id, action, page_size, offset);
        }
    } else if (user_id > 0) {
        auto cnt = db_.DB().query_s<std::tuple<int64_t>>("SELECT count(*) FROM audit_logs WHERE user_id=?", user_id);
        if (!cnt.empty()) result.total = std::get<0>(cnt[0]);
        result.items = db_.DB().query_s<AuditLog>("user_id=? ORDER BY id DESC LIMIT ? OFFSET ?", user_id, page_size, offset);
    } else if (!action.empty()) {
        auto cnt = db_.DB().query_s<std::tuple<int64_t>>("SELECT count(*) FROM audit_logs WHERE action=?", action);
        if (!cnt.empty()) result.total = std::get<0>(cnt[0]);
        result.items = db_.DB().query_s<AuditLog>("action=? ORDER BY id DESC LIMIT ? OFFSET ?", action, page_size, offset);
    } else {
        auto cnt = db_.DB().query_s<std::tuple<int64_t>>("SELECT count(*) FROM audit_logs");
        if (!cnt.empty()) result.total = std::get<0>(cnt[0]);
        result.items = db_.DB().query_s<AuditLog>("1=1 ORDER BY id DESC LIMIT ? OFFSET ?", page_size, offset);
    }

    return result;
}

} // namespace nova
