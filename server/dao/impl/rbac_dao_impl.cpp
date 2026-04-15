#include "rbac_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
std::vector<std::string> RbacDaoImplT<DbMgr>::GetUserPermissions(int64_t user_id) {
    auto rows = db_.DB().query_s<std::tuple<std::string>>(
        "SELECT DISTINCT p.code FROM permissions p "
        "JOIN role_permissions rp ON rp.permission_id = p.id "
        "JOIN user_roles ur ON ur.role_id = rp.role_id "
        "WHERE ur.user_id = ?", user_id);

    std::vector<std::string> perms;
    perms.reserve(rows.size());
    for (auto& [code] : rows) {
        perms.push_back(std::move(code));
    }
    return perms;
}

template <typename DbMgr>
bool RbacDaoImplT<DbMgr>::HasPermission(int64_t user_id, const std::string& code) {
    auto rows = db_.DB().query_s<std::tuple<int>>(
        "SELECT 1 FROM permissions p "
        "JOIN role_permissions rp ON rp.permission_id = p.id "
        "JOIN user_roles ur ON ur.role_id = rp.role_id "
        "WHERE ur.user_id = ? AND p.code = ? LIMIT 1", user_id, code);
    return !rows.empty();
}

// 显式实例化
template class RbacDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class RbacDaoImplT<MysqlDbManager>;
#endif

} // namespace nova
