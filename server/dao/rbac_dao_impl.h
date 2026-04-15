#pragma once

#include "rbac_dao.h"
#include "db_manager.h"

namespace nova {

class RbacDaoImpl : public RbacDao {
public:
    explicit RbacDaoImpl(DbManager& db) : db_(db) {}

    std::vector<std::string> GetUserPermissions(int64_t user_id) override;
    bool HasPermission(int64_t user_id, const std::string& code) override;

private:
    DbManager& db_;
};

} // namespace nova
