#pragma once

#include "../admin_account_dao.h"

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class AdminAccountDaoImplT : public AdminAccountDao {
public:
    explicit AdminAccountDaoImplT(DbMgr& db) : db_(db) {}

    std::optional<Admin> FindByUid(const std::string& uid) override;
    std::optional<Admin> FindById(int64_t id) override;
    bool Insert(Admin& admin) override;
    bool UpdatePassword(int64_t id, const std::string& password_hash) override;
    PaginatedAdmins ListAdmins(const std::string& keyword, int page, int page_size) override;
    bool SoftDelete(int64_t id) override;
    bool UpdateStatus(int64_t id, int status) override;

private:
    DbMgr& db_;
};

}  // namespace nova
