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

    // 吐销某管理员所有 session
    virtual bool RevokeByAdmin(int64_t admin_id) = 0;

    // 吊销单个 token
    virtual bool RevokeByTokenHash(const std::string& token_hash) = 0;
    // 清理已过期的 session 记录（expires_at < now）。
    // 返回被删除的行数，失败返回 -1。供后台定时任务调用。
    virtual int  PurgeExpired(int64_t now_sec) = 0;};

}  // namespace nova
