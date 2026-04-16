#pragma once

#include "../user_dao.h"

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class UserDaoImplT : public UserDao {
public:
    explicit UserDaoImplT(DbMgr& db) : db_(db) {}

    std::optional<User> FindByUid(const std::string& uid) override;
    std::optional<User> FindById(int64_t id) override;
    UserListResult ListUsers(const std::string& keyword, int status, int page, int page_size) override;
    bool Insert(User& user) override;
    bool UpdateStatus(int64_t id, int8_t status) override;
    bool UpdatePassword(int64_t id, const std::string& password_hash) override;
    bool SoftDelete(int64_t id) override;
    std::vector<UserDevice> ListDevicesByUser(int64_t user_id) override;

private:
    DbMgr& db_;
};

}  // namespace nova
