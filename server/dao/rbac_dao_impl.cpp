#include "rbac_dao_impl.h"

namespace nova {

std::vector<std::string> RbacDaoImpl::GetUserPermissions(int64_t user_id) {
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

bool RbacDaoImpl::HasPermission(int64_t user_id, const std::string& code) {
    auto rows = db_.DB().query_s<std::tuple<int>>(
        "SELECT 1 FROM permissions p "
        "JOIN role_permissions rp ON rp.permission_id = p.id "
        "JOIN user_roles ur ON ur.role_id = rp.role_id "
        "WHERE ur.user_id = ? AND p.code = ? LIMIT 1", user_id, code);
    return !rows.empty();
}

} // namespace nova
