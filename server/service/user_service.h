#pragma once

#include "service_base.h"
#include "../core/rate_limiter.h"

namespace nova {

// 用户服务
// 职责：登录认证（密码校验 + 频率限制）、登出、心跳
class UserService : public ServiceBase {
public:
    explicit UserService(ServerContext& ctx)
        : ServiceBase(ctx)
        , login_limiter_(5, std::chrono::seconds(60)) {}

    void HandleLogin(ConnectionPtr conn, Packet& pkt);
    void HandleLogout(ConnectionPtr conn, Packet& pkt);
    void HandleHeartbeat(ConnectionPtr conn, Packet& pkt);

private:
    RateLimiter login_limiter_;  // 5 次失败 / 60 秒窗口
};

} // namespace nova
