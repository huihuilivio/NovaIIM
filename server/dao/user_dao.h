#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include "../model/types.h"

namespace nova {

struct UserListResult {
    std::vector<User> items;
    int64_t total = 0;
};

// 用户 DAO 接口
class UserDao {
public:
    virtual ~UserDao() = default;

    virtual std::optional<User> FindByUid(const std::string& uid) = 0;
    virtual std::optional<User> FindByEmail(const std::string& email) = 0;
    virtual std::optional<User> FindById(int64_t id)              = 0;

    // 分页查询，keyword 可匹配 uid/nickname，status=-1 表示不过滤
    virtual UserListResult ListUsers(const std::string& keyword, int status, int page, int page_size) = 0;

    // 按昵称模糊搜索（用户端，脱敏，最多返回 limit 条）
    virtual std::vector<User> SearchByNickname(const std::string& keyword, int limit = 20) = 0;

    virtual bool Insert(User& user)                                                      = 0;  // 成功后 user.id 被填充
    virtual std::optional<int64_t> UpdateStatus(const std::string& uid, int8_t status)     = 0;
    virtual std::optional<int64_t> UpdatePassword(const std::string& uid, const std::string& password_hash) = 0;
    virtual std::optional<int64_t> UpdateAvatar(const std::string& uid, const std::string& avatar)          = 0;
    virtual std::optional<int64_t> UpdateNickname(const std::string& uid, const std::string& nickname)      = 0;
    virtual std::optional<int64_t> SoftDelete(const std::string& uid)                      = 0;  // status → 3

    // 查询用户设备列表
    virtual std::vector<UserDevice> ListDevicesByUser(const std::string& uid) = 0;

    // 登录时更新/插入设备记录
    virtual void UpsertDevice(const std::string& uid, const std::string& device_id, const std::string& device_type) = 0;
};

}  // namespace nova
