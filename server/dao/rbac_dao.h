#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nova {

class RbacDao {
public:
    virtual ~RbacDao() = default;

    // 获取用户所有权限 code 列表（通过 user_roles + role_permissions + permissions 联查）
    virtual std::vector<std::string> GetUserPermissions(int64_t user_id) = 0;

    // 判断用户是否拥有某权限
    virtual bool HasPermission(int64_t user_id, const std::string& code) = 0;
};

}  // namespace nova
