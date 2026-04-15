#pragma once

#include <memory>
#include <string>

#include <hv/HttpServer.h>
#include <hv/HttpService.h>

namespace nova {

class ServerContext;

// Admin HTTP 管理面板（独立端口，不走 Gateway TCP 协议）
// 职责：健康检查、运行时指标、连接管理
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

    // 鉴权中间件（JWT）
    static int AuthMiddleware(HttpRequest* req, HttpResponse* resp, const std::string& secret);

    // --- handler ---
    int HandleHealthz(HttpRequest* req, HttpResponse* resp);
    int HandleStats(HttpRequest* req, HttpResponse* resp);
    int HandleKickUser(HttpRequest* req, HttpResponse* resp);

    ServerContext& ctx_;
    Options opts_;
    hv::HttpService service_;
    std::unique_ptr<hv::HttpServer> server_;
};

} // namespace nova
