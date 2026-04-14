#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include "../model/types.h"

namespace nova {

// 用户 DAO
class UserDao {
public:
    virtual ~UserDao() = default;

    // 根据 uid 查询用户
    virtual std::optional<User> FindByUid(const std::string& uid) = 0;

    // 验证密码（返回用户 id，失败返回 -1）
    virtual int64_t Authenticate(const std::string& uid, const std::string& password) = 0;
};

} // namespace nova
