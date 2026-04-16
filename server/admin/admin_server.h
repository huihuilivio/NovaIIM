#pragma once

#include <memory>
#include <string>

#include <hv/HttpServer.h>
#include <hv/HttpService.h>

namespace nova {

class ServerContext;
class DaoFactory;

// Admin HTTP 管理面板（独立端口，不走 Gateway TCP 协议）
class AdminServer {
public:
    struct Options {
        int         port        = 9091;
        std::string jwt_secret;     // JWT HMAC 密钥，空则不鉴权
        int         jwt_expires = 86400;
    };

    explicit AdminServer(ServerContext& ctx);
    ~AdminServer();

    AdminServer(const AdminServer&) = delete;
    AdminServer& operator=(const AdminServer&) = delete;

    int  Start(const Options& opts);
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

    // --- messages ---
    int HandleListMessages(HttpRequest* req, HttpResponse* resp);
    int HandleRecallMessage(HttpRequest* req, HttpResponse* resp);

    // --- audit ---
    int HandleListAuditLogs(HttpRequest* req, HttpResponse* resp);

    // 审计日志写入助手
    void WriteAuditLog(int64_t admin_id, const std::string& action,
                       const std::string& target_type, int64_t target_id,
                       const std::string& detail, const std::string& ip);

    // 从请求中获取客户端 IP（根据 trust_proxy 配置决定是否信任代理头）
    std::string GetClientIp(HttpRequest* req) const;

    ServerContext& ctx_;
    Options opts_;
    hv::HttpService service_;
    std::unique_ptr<hv::HttpServer> server_;
};

} // namespace nova
