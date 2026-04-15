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

private:
    DbMgr& db_;
};

} // namespace nova
