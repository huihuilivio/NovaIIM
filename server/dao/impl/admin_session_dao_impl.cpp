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
    if (res.empty()) return false;
    return res[0].revoked == 1;
}

template <typename DbMgr>
bool AdminSessionDaoImplT<DbMgr>::RevokeByAdmin(int64_t admin_id) {
    auto sessions = db_.DB().query_s<AdminSession>("admin_id=? AND revoked=0", admin_id);
    for (auto& s : sessions) {
        s.revoked = 1;
        db_.DB().update_some<&AdminSession::revoked>(s);
    }
    return true;
}

template <typename DbMgr>
bool AdminSessionDaoImplT<DbMgr>::RevokeByTokenHash(const std::string& token_hash) {
    auto sessions = db_.DB().query_s<AdminSession>("token_hash=?", token_hash);
    if (sessions.empty()) return false;
    sessions[0].revoked = 1;
    return db_.DB().update_some<&AdminSession::revoked>(sessions[0]) == 1;
}

// 显式实例化
template class AdminSessionDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class AdminSessionDaoImplT<MysqlDbManager>;
#endif

} // namespace nova
