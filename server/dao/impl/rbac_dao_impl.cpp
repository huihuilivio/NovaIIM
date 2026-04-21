#include "rbac_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif
#include "../../model/types.h"

namespace nova {

template <typename DbMgr>
std::vector<std::string> RbacDaoImplT<DbMgr>::GetUserPermissions(int64_t user_id) {
    auto rows = db_.DB().query_s<std::tuple<std::string>>(
        "SELECT DISTINCT p.code FROM permissions p "
        "JOIN role_permissions rp ON rp.permission_id = p.id "
        "JOIN admin_roles ar ON ar.role_id = rp.role_id "
        "WHERE ar.admin_id = ?",
        user_id);

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
        "JOIN admin_roles ar ON ar.role_id = rp.role_id "
        "WHERE ar.admin_id = ? AND p.code = ? LIMIT 1",
        user_id, code);
    return !rows.empty();
}

// ---- 角色管理 ----

template <typename DbMgr>
std::vector<RoleWithPermissions> RbacDaoImplT<DbMgr>::ListRoles() {
    auto roles = db_.DB().query_s<Role>("");
    std::vector<RoleWithPermissions> result;
    result.reserve(roles.size());
    for (auto& r : roles) {
        RoleWithPermissions rp;
        rp.id          = r.id;
        rp.name        = r.name;
        rp.description = r.description;
        rp.created_at  = r.created_at;
        // 查询该角色的权限
        auto perms = db_.DB().query_s<std::tuple<std::string>>(
            "SELECT p.code FROM permissions p "
            "JOIN role_permissions rp ON rp.permission_id = p.id "
            "WHERE rp.role_id = ?",
            r.id);
        for (auto& [code] : perms) {
            rp.permissions.push_back(std::move(code));
        }
        result.push_back(std::move(rp));
    }
    return result;
}

template <typename DbMgr>
bool RbacDaoImplT<DbMgr>::CreateRole(const std::string& name, const std::string& description) {
    Role role;
    role.name        = name;
    role.code        = name;  // code = name for simplicity
    role.description = description;
    auto id = db_.DB().get_insert_id_after_insert(role);
    return id > 0;
}

template <typename DbMgr>
bool RbacDaoImplT<DbMgr>::UpdateRole(int64_t role_id, const std::string& description) {
    auto res = db_.DB().query_s<Role>("id=?", role_id);
    if (res.empty()) return false;
    res[0].description = description;
    return db_.DB().template update_some<&Role::description>(res[0]) == 1;
}

template <typename DbMgr>
bool RbacDaoImplT<DbMgr>::DeleteRole(int64_t role_id) {
    // 先删除关联的 role_permissions 和 admin_roles
    auto id_s = std::to_string(role_id);
    db_.DB().execute("DELETE FROM role_permissions WHERE role_id = " + id_s);
    db_.DB().execute("DELETE FROM admin_roles WHERE role_id = " + id_s);
    db_.DB().execute("DELETE FROM roles WHERE id = " + id_s);
    return true;
}

template <typename DbMgr>
bool RbacDaoImplT<DbMgr>::SetRolePermissions(int64_t role_id, const std::vector<std::string>& perm_codes) {
    // 删除旧的权限关联
    db_.DB().execute("DELETE FROM role_permissions WHERE role_id = " + std::to_string(role_id));
    // 插入新的
    for (const auto& code : perm_codes) {
        auto perm_rows = db_.DB().query_s<std::tuple<int64_t>>(
            "SELECT id FROM permissions WHERE code = ?", code);
        if (perm_rows.empty()) continue;
        int64_t perm_id = std::get<0>(perm_rows[0]);
        RolePermission rp;
        rp.role_id       = role_id;
        rp.permission_id = perm_id;
        db_.DB().insert(rp);
    }
    return true;
}

template <typename DbMgr>
std::vector<std::string> RbacDaoImplT<DbMgr>::ListPermissions() {
    auto rows = db_.DB().query_s<std::tuple<std::string>>(
        "SELECT code FROM permissions ORDER BY id");
    std::vector<std::string> result;
    result.reserve(rows.size());
    for (auto& [code] : rows) {
        result.push_back(std::move(code));
    }
    return result;
}

template <typename DbMgr>
std::vector<std::string> RbacDaoImplT<DbMgr>::GetAdminRoles(int64_t admin_id) {
    auto rows = db_.DB().query_s<std::tuple<std::string>>(
        "SELECT r.name FROM roles r "
        "JOIN admin_roles ar ON ar.role_id = r.id "
        "WHERE ar.admin_id = ?",
        admin_id);
    std::vector<std::string> result;
    result.reserve(rows.size());
    for (auto& [name] : rows) {
        result.push_back(std::move(name));
    }
    return result;
}

template <typename DbMgr>
bool RbacDaoImplT<DbMgr>::AssignAdminRole(int64_t admin_id, int64_t role_id) {
    // 检查是否已有
    auto existing = db_.DB().query_s<std::tuple<int>>(
        "SELECT 1 FROM admin_roles WHERE admin_id = ? AND role_id = ?",
        admin_id, role_id);
    if (!existing.empty()) return true;
    AdminRole ar;
    ar.admin_id = admin_id;
    ar.role_id  = role_id;
    return db_.DB().insert(ar) == 1;
}

template <typename DbMgr>
bool RbacDaoImplT<DbMgr>::RemoveAdminRole(int64_t admin_id, int64_t role_id) {
    db_.DB().execute("DELETE FROM admin_roles WHERE admin_id = " + std::to_string(admin_id) +
                     " AND role_id = " + std::to_string(role_id));
    return true;
}

// 显式实例化
template class RbacDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class RbacDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
