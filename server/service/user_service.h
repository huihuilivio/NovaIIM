#pragma once

#include "service_base.h"
#include "../core/rate_limiter.h"

namespace nova {

// 用户服务
// 职责：登录认证、登出、心跳、用户搜索、个人资料 CRUD
class UserService : public ServiceBase {
public:
    explicit UserService(ServerContext& ctx)
        : ServiceBase(ctx),
          login_limiter_(ctx.config().server.login_max_attempts, std::chrono::seconds(ctx.config().server.login_window_secs)),
          register_limiter_(ctx.config().server.login_max_attempts, std::chrono::seconds(ctx.config().server.login_window_secs)) {}

    void HandleLogin(ConnectionPtr conn, Packet& pkt);
    void HandleRegister(ConnectionPtr conn, Packet& pkt);
    void HandleLogout(ConnectionPtr conn, Packet& pkt);
    void HandleHeartbeat(ConnectionPtr conn, Packet& pkt);
    void HandleSearchUser(ConnectionPtr conn, Packet& pkt);
    void HandleGetProfile(ConnectionPtr conn, Packet& pkt);
    void HandleUpdateProfile(ConnectionPtr conn, Packet& pkt);

private:
    RateLimiter login_limiter_;     // 从 config.server.login_* 初始化
    RateLimiter register_limiter_;  // 注册频率限制（同参数）
};

}  // namespace nova
