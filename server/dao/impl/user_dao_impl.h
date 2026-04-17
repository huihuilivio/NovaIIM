#pragma once

#include "../user_dao.h"

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class UserDaoImplT : public UserDao {
public:
    explicit UserDaoImplT(DbMgr& db) : db_(db) {}

    std::optional<User> FindByUid(const std::string& uid) override;
    std::optional<User> FindByEmail(const std::string& email) override;
    std::optional<User> FindById(int64_t id) override;
    std::vector<User> FindByIds(const std::vector<int64_t>& ids) override;
    UserListResult ListUsers(const std::string& keyword, int status, int page, int page_size) override;
    std::vector<User> SearchByNickname(const std::string& keyword, int limit) override;
    bool Insert(User& user) override;
    std::optional<int64_t> UpdateStatus(const std::string& uid, int8_t status) override;
    std::optional<int64_t> UpdatePassword(const std::string& uid, const std::string& password_hash) override;
    std::optional<int64_t> UpdateAvatar(const std::string& uid, const std::string& avatar) override;
    std::optional<int64_t> UpdateNickname(const std::string& uid, const std::string& nickname) override;
    std::optional<int64_t> SoftDelete(const std::string& uid) override;
    std::vector<UserDevice> ListDevicesByUser(const std::string& uid) override;
    void UpsertDevice(const std::string& uid, const std::string& device_id, const std::string& device_type) override;

private:
    DbMgr& db_;
};

}  // namespace nova
