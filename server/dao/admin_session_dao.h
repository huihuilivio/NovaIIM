#pragma once

#include <cstdint>
#include <string>
#include "../model/types.h"

namespace nova {

class AdminSessionDao {
public:
    virtual ~AdminSessionDao() = default;

    virtual bool Insert(const AdminSession& session) = 0;

    // 检查 token 是否已被吊销
    virtual bool IsRevoked(const std::string& token_hash) = 0;

    // 吊销某用户所有 session
    virtual bool RevokeByUser(int64_t user_id) = 0;

    // 吊销单个 token
    virtual bool RevokeByTokenHash(const std::string& token_hash) = 0;
};

} // namespace nova
