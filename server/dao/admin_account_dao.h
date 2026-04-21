#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include "../model/types.h"

namespace nova {

struct PaginatedAdmins {
    std::vector<Admin> items;
    int64_t total = 0;
};

// 管理员账户 DAO（独立于 IM 用户）
class AdminAccountDao {
public:
    virtual ~AdminAccountDao() = default;

    virtual std::optional<Admin> FindByUid(const std::string& uid)            = 0;
    virtual std::optional<Admin> FindById(int64_t id)                         = 0;
    virtual bool Insert(Admin& admin)                                         = 0;  // 成功后 admin.id 被填充
    virtual bool UpdatePassword(int64_t id, const std::string& password_hash) = 0;

    // ---- 新增: Admin CRUD ----
    virtual PaginatedAdmins ListAdmins(const std::string& keyword, int page, int page_size) = 0;
    virtual bool SoftDelete(int64_t id)                                                     = 0;
    virtual bool UpdateStatus(int64_t id, int status)                                       = 0;
};

}  // namespace nova
