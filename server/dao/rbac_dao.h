#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nova {

struct Role;
struct Permission;

// 角色 + 附带权限列表（用于 API 返回）
struct RoleWithPermissions {
    int64_t id = 0;
    std::string name;
    std::string description;
    std::string created_at;
    std::vector<std::string> permissions;  // permission codes
};

class RbacDao {
public:
    virtual ~RbacDao() = default;

    // 获取用户所有权限 code 列表（通过 admin_roles + role_permissions + permissions 联查）
    virtual std::vector<std::string> GetUserPermissions(int64_t user_id) = 0;

    // 判断用户是否拥有某权限
    virtual bool HasPermission(int64_t user_id, const std::string& code) = 0;

    // ---- 角色管理 ----
    virtual std::vector<RoleWithPermissions> ListRoles() = 0;
    virtual bool CreateRole(const std::string& name, const std::string& description) = 0;
    virtual bool UpdateRole(int64_t role_id, const std::string& description) = 0;
    virtual bool DeleteRole(int64_t role_id) = 0;
    virtual bool SetRolePermissions(int64_t role_id, const std::vector<std::string>& perm_codes) = 0;

    // ---- 权限查询 ----
    virtual std::vector<std::string> ListPermissions() = 0;

    // ---- 管理员-角色关联 ----
    virtual std::vector<std::string> GetAdminRoles(int64_t admin_id) = 0;
    virtual bool AssignAdminRole(int64_t admin_id, int64_t role_id) = 0;
    virtual bool RemoveAdminRole(int64_t admin_id, int64_t role_id) = 0;
};

}  // namespace nova
