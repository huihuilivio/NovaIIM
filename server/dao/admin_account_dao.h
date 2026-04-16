#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include "../model/types.h"

namespace nova {

// 管理员账户 DAO（独立于 IM 用户）
class AdminAccountDao {
public:
    virtual ~AdminAccountDao() = default;

    virtual std::optional<Admin> FindByUid(const std::string& uid)            = 0;
    virtual std::optional<Admin> FindById(int64_t id)                         = 0;
    virtual bool Insert(Admin& admin)                                         = 0;  // 成功后 admin.id 被填充
    virtual bool UpdatePassword(int64_t id, const std::string& password_hash) = 0;
};

}  // namespace nova
