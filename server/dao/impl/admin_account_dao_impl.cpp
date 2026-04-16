#include "admin_account_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
std::optional<Admin> AdminAccountDaoImplT<DbMgr>::FindByUid(const std::string& uid) {
    auto res = db_.DB().query_s<Admin>("uid=? AND status!=3", uid);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::optional<Admin> AdminAccountDaoImplT<DbMgr>::FindById(int64_t id) {
    auto res = db_.DB().query_s<Admin>("id=? AND status!=3", id);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
bool AdminAccountDaoImplT<DbMgr>::Insert(Admin& admin) {
    auto id = db_.DB().get_insert_id_after_insert(admin);
    if (id == 0)
        return false;
    admin.id = static_cast<int64_t>(id);
    return true;
}

template <typename DbMgr>
bool AdminAccountDaoImplT<DbMgr>::UpdatePassword(int64_t id, const std::string& password_hash) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<Admin>("id=?", id);
    if (res.empty())
        return false;
    res[0].password_hash = password_hash;
    return conn.template update_some<&Admin::password_hash>(res[0]) == 1;
}

// 显式实例化
template class AdminAccountDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class AdminAccountDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
