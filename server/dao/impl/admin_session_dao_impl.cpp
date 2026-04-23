#include "admin_session_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
bool AdminSessionDaoImplT<DbMgr>::Insert(const AdminSession& session) {
    return db_.DB().insert(session) == 1;
}

template <typename DbMgr>
bool AdminSessionDaoImplT<DbMgr>::IsRevoked(const std::string& token_hash) {
    auto res = db_.DB().query_s<AdminSession>("token_hash=?", token_hash);
    if (res.empty())
        return true;  // session not found → treat as revoked (fail-closed)
    return res[0].revoked == static_cast<int>(SessionRevoked::Revoked);
}

template <typename DbMgr>
bool AdminSessionDaoImplT<DbMgr>::RevokeByAdmin(int64_t admin_id) {
    std::string sql = "UPDATE admin_sessions SET revoked = 1 WHERE admin_id = " + std::to_string(admin_id) +  // Revoked
                      " AND revoked = 0";  // Valid
    return db_.DB().execute(sql);
}

template <typename DbMgr>
bool AdminSessionDaoImplT<DbMgr>::RevokeByTokenHash(const std::string& token_hash) {
    auto&& conn   = db_.DB();
    auto sessions = conn.query_s<AdminSession>("token_hash=?", token_hash);
    if (sessions.empty())
        return false;
    sessions[0].revoked = static_cast<int>(SessionRevoked::Revoked);
    return conn.update_some<&AdminSession::revoked>(sessions[0]) == 1;
}

template <typename DbMgr>
int AdminSessionDaoImplT<DbMgr>::PurgeExpired(int64_t now_sec) {
    // 直接拼 SQL： expires_at 按约定存储为 epoch 秒，where 内只有数字常量，无注入风险。
    std::string sql = "DELETE FROM admin_sessions WHERE expires_at < " + std::to_string(now_sec);
    if (!db_.DB().execute(sql)) return -1;
    // ormpp 不返回 affected rows；不关心具体数，返回 0 表示成功。
    return 0;
}

// 显式实例化
template class AdminSessionDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class AdminSessionDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
