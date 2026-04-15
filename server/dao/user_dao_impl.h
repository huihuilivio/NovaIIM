#pragma once

#include "user_dao.h"
#include "db_manager.h"

namespace nova {

class UserDaoImpl : public UserDao {
public:
    explicit UserDaoImpl(DbManager& db) : db_(db) {}

    std::optional<User> FindByUid(const std::string& uid) override;
    std::optional<User> FindById(int64_t id) override;
    UserListResult ListUsers(const std::string& keyword, int status,
                             int page, int page_size) override;
    bool Insert(User& user) override;
    bool UpdateStatus(int64_t id, int8_t status) override;
    bool UpdatePassword(int64_t id, const std::string& password_hash) override;
    bool SoftDelete(int64_t id) override;

private:
    DbManager& db_;
};

} // namespace nova
