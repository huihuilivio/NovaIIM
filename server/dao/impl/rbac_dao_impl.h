#pragma once

#include "../rbac_dao.h"

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class RbacDaoImplT : public RbacDao {
public:
    explicit RbacDaoImplT(DbMgr& db) : db_(db) {}

    std::vector<std::string> GetUserPermissions(int64_t user_id) override;
    bool HasPermission(int64_t user_id, const std::string& code) override;

    std::vector<RoleWithPermissions> ListRoles() override;
    bool CreateRole(const std::string& name, const std::string& description) override;
    bool UpdateRole(int64_t role_id, const std::string& description) override;
    bool DeleteRole(int64_t role_id) override;
    bool SetRolePermissions(int64_t role_id, const std::vector<std::string>& perm_codes) override;
    std::vector<std::string> ListPermissions() override;
    std::vector<std::string> GetAdminRoles(int64_t admin_id) override;
    bool AssignAdminRole(int64_t admin_id, int64_t role_id) override;
    bool RemoveAdminRole(int64_t admin_id, int64_t role_id) override;

private:
    DbMgr& db_;
};

}  // namespace nova
