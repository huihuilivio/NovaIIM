#include "audit_log_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
bool AuditLogDaoImplT<DbMgr>::Insert(const AuditLog& log) {
    return db_.DB().insert(log) == 1;
}

template <typename DbMgr>
AuditLogListResult AuditLogDaoImplT<DbMgr>::List(int64_t admin_id, const std::string& action,
                                                 const std::string& start_time, const std::string& end_time, int page,
                                                 int page_size) {
    AuditLogListResult result;
    int offset = (page - 1) * page_size;

    static constexpr auto kCountSql =
        "SELECT count(*) FROM audit_logs WHERE "
        "(? = 0 OR admin_id = ?) AND "
        "(? = '' OR action = ?) AND "
        "(? = '' OR created_at >= ?) AND "
        "(? = '' OR created_at <= ?)";

    // ormpp master 的 query_s 在检测到条件含 "order by"/"limit" 时
    // 会错误地吃掉 WHERE 关键字，因此这里用完整 SELECT 语句绕过
    static constexpr auto kSelectSql =
        "SELECT id, admin_id, action, target_type, target_id, detail, ip_address, created_at "
        "FROM audit_logs WHERE "
        "(? = 0 OR admin_id = ?) AND "
        "(? = '' OR action = ?) AND "
        "(? = '' OR created_at >= ?) AND "
        "(? = '' OR created_at <= ?) "
        "ORDER BY id DESC LIMIT ? OFFSET ?";

    auto&& conn = db_.DB();

    auto cnt = conn.query_s<std::tuple<int64_t>>(kCountSql, admin_id, admin_id, action, action, start_time, start_time,
                                                 end_time, end_time);
    if (!cnt.empty())
        result.total = std::get<0>(cnt[0]);

    result.items = conn.query_s<AuditLog>(kSelectSql, admin_id, admin_id, action, action, start_time, start_time,
                                          end_time, end_time, page_size, offset);

    return result;
}

// 显式实例化
template class AuditLogDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class AuditLogDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
