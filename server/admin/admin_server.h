#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <hv/HttpServer.h>
#include <hv/HttpService.h>

#include "../core/rate_limiter.h"

namespace nova {

class ServerContext;
class DaoFactory;

// Admin HTTP 管理面板（独立端口，不走 Gateway TCP 协议）
class AdminServer {
public:
    struct Options {
        int port = 9091;
        std::string jwt_secret;  // JWT HMAC 密钥，空则不鉴权
        int jwt_expires = 86400;
    };

    explicit AdminServer(ServerContext& ctx);
    ~AdminServer();

    AdminServer(const AdminServer&)            = delete;
    AdminServer& operator=(const AdminServer&) = delete;

    int Start(const Options& opts);
    void Stop();

private:
    void RegisterRoutes(const Options& opts);

    // 鉴权中间件（JWT + 黑名单 + RBAC 注入）
    int AuthMiddleware(HttpRequest* req, HttpResponse* resp);

    // --- auth ---
    int HandleLogin(HttpRequest* req, HttpResponse* resp);
    int HandleLogout(HttpRequest* req, HttpResponse* resp);
    int HandleMe(HttpRequest* req, HttpResponse* resp);

    // --- dashboard ---
    int HandleHealthz(HttpRequest* req, HttpResponse* resp);
    int HandleStats(HttpRequest* req, HttpResponse* resp);

    // --- user management ---
    int HandleListUsers(HttpRequest* req, HttpResponse* resp);
    int HandleCreateUser(HttpRequest* req, HttpResponse* resp);
    int HandleGetUser(HttpRequest* req, HttpResponse* resp);
    int HandleDeleteUser(HttpRequest* req, HttpResponse* resp);
    int HandleResetPassword(HttpRequest* req, HttpResponse* resp);
    int HandleBanUser(HttpRequest* req, HttpResponse* resp);
    int HandleUnbanUser(HttpRequest* req, HttpResponse* resp);
    int HandleKickUser(HttpRequest* req, HttpResponse* resp);

    // 踢下线：发送 KickNotify + Remove + Close
    int KickAllConns(int64_t user_id);

    // --- messages ---
    int HandleListMessages(HttpRequest* req, HttpResponse* resp);
    int HandleRecallMessage(HttpRequest* req, HttpResponse* resp);

    // --- audit ---
    int HandleListAuditLogs(HttpRequest* req, HttpResponse* resp);

    // --- admin management ---
    int HandleListAdmins(HttpRequest* req, HttpResponse* resp);
    int HandleCreateAdmin(HttpRequest* req, HttpResponse* resp);
    int HandleDeleteAdmin(HttpRequest* req, HttpResponse* resp);
    int HandleResetAdminPassword(HttpRequest* req, HttpResponse* resp);
    int HandleEnableAdmin(HttpRequest* req, HttpResponse* resp);
    int HandleDisableAdmin(HttpRequest* req, HttpResponse* resp);
    int HandleSetAdminRoles(HttpRequest* req, HttpResponse* resp);

    // --- role management ---
    int HandleListRoles(HttpRequest* req, HttpResponse* resp);
    int HandleCreateRole(HttpRequest* req, HttpResponse* resp);
    int HandleUpdateRole(HttpRequest* req, HttpResponse* resp);
    int HandleDeleteRole(HttpRequest* req, HttpResponse* resp);
    int HandleListPermissions(HttpRequest* req, HttpResponse* resp);

    // 审计日志写入助手
    void WriteAuditLog(int64_t admin_id, const std::string& action, const std::string& target_type, int64_t target_id,
                       const std::string& detail, const std::string& ip);

    // 从请求中获取客户端 IP（根据 trust_proxy 配置决定是否信任代理头）
    std::string GetClientIp(HttpRequest* req) const;

    ServerContext& ctx_;
    Options opts_;
    hv::HttpService service_;
    std::unique_ptr<hv::HttpServer> server_;
    RateLimiter login_limiter_;

    // 过期 session 定期清理线程（每 10 分钟执行一次 DELETE WHERE expires_at < now）
    void SessionPurgerLoop();
    std::thread            purger_thread_;
    std::mutex             purger_mu_;
    std::condition_variable purger_cv_;
    std::atomic<bool>      purger_stop_{false};

    // ── RBAC TTL 缓存：减少每请求查询 permissions + admin 账户状态 ──
    struct RbacCacheEntry {
        std::vector<std::string> perms;
        int  account_status = 0;
        std::chrono::steady_clock::time_point expires_at;
    };
    static constexpr std::chrono::seconds kRbacCacheTtl{30};
    static constexpr size_t kRbacCacheMaxSize = 1024;  // 容量上限，超过则清理过期项（或全部）以防内存无限增长
    std::mutex rbac_cache_mu_;
    std::unordered_map<int64_t, RbacCacheEntry> rbac_cache_;
    // role/admin 状态变更时调用，发生在修改接口成功后。
    void InvalidateRbacCache(int64_t admin_id);
    void InvalidateRbacCacheAll();
};

}  // namespace nova
