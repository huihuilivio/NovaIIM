#include "admin_server.h"
#include "http_helper.h"
#include "jwt_utils.h"
#include "../core/server_context.h"
#include "../net/conn_manager.h"
#include "../core/logger.h"

#include <hv/json.hpp>

namespace nova {

static constexpr const char* kLogTag = "Admin";

AdminServer::AdminServer(ServerContext& ctx) : ctx_(ctx) {}

AdminServer::~AdminServer() {
    Stop();
}

int AdminServer::Start(const Options& opts) {
    if (server_) {
        NOVA_NLOG_WARN(kLogTag, "already started");
        return -1;
    }

    opts_ = opts;
    RegisterRoutes(opts);

    server_ = std::make_unique<hv::HttpServer>(&service_);
    server_->setThreadNum(1);  // admin 流量小，1 个线程足够
    server_->setPort(opts.port);

    int ret = server_->start();
    if (ret != 0) {
        NOVA_NLOG_ERROR(kLogTag, "failed to start on port {}", opts.port);
        server_.reset();
        return -1;
    }

    NOVA_NLOG_INFO(kLogTag, "HTTP admin server started on port {}", opts.port);
    return 0;
}

void AdminServer::Stop() {
    if (server_) {
        server_->stop();
        server_.reset();
        NOVA_NLOG_INFO(kLogTag, "stopped");
    }
}

void AdminServer::RegisterRoutes(const Options& opts) {
    // JWT 鉴权中间件（jwt_secret 非空时启用）
    if (!opts.jwt_secret.empty()) {
        std::string secret = opts.jwt_secret;
        service_.Use([secret](HttpRequest* req, HttpResponse* resp) -> int {
            return AuthMiddleware(req, resp, secret);
        });
    }

    service_.GET("/healthz", [this](HttpRequest* req, HttpResponse* resp) {
        return HandleHealthz(req, resp);
    });

    service_.GET("/api/v1/dashboard/stats", [this](HttpRequest* req, HttpResponse* resp) {
        return HandleStats(req, resp);
    });

    // 保留旧路径兼容
    service_.GET("/api/v1/stats", [this](HttpRequest* req, HttpResponse* resp) {
        return HandleStats(req, resp);
    });

    service_.POST("/api/v1/kick", [this](HttpRequest* req, HttpResponse* resp) {
        return HandleKickUser(req, resp);
    });
}

int AdminServer::AuthMiddleware(HttpRequest* req, HttpResponse* resp, const std::string& secret) {
    // 清除客户端可能伪造的内部头
    req->SetHeader("X-Nova-User-Id", "");
    req->SetHeader("X-Nova-Permissions", "");

    // 不需要鉴权的路径
    if (req->path == "/healthz" || req->path == "/api/v1/auth/login") {
        return HTTP_STATUS_NEXT;
    }

    std::string auth = req->GetHeader("Authorization");
    constexpr std::string_view kBearer = "Bearer ";
    if (auth.size() <= kBearer.size() || auth.substr(0, kBearer.size()) != kBearer) {
        JsonError(resp, ApiCode::kUnauthorized, "missing or invalid token", 401);
        return 401;
    }

    auto token = std::string_view(auth).substr(kBearer.size());
    auto claims = JwtUtils::Verify(token, secret);
    if (!claims) {
        JsonError(resp, ApiCode::kUnauthorized, "invalid or expired token", 401);
        return 401;
    }

    // TODO: Phase 2 — 查 admin_sessions 黑名单 + 注入 permissions
    // 注入 user_id 到请求头供 handler 使用
    req->SetHeader("X-Nova-User-Id", std::to_string(claims->user_id));

    return HTTP_STATUS_NEXT;
}

// GET /healthz
int AdminServer::HandleHealthz(HttpRequest* /*req*/, HttpResponse* resp) {
    return JsonOk(resp, {{"status", "ok"}});
}

// GET /api/v1/dashboard/stats
int AdminServer::HandleStats(HttpRequest* /*req*/, HttpResponse* resp) {
    nlohmann::json data;
    data["connections"]    = ctx_.connection_count();
    data["online_users"]   = ctx_.online_user_count();
    data["messages_in"]    = ctx_.total_messages_in();
    data["messages_out"]   = ctx_.total_messages_out();
    data["bad_packets"]    = ctx_.bad_packets();
    data["uptime_seconds"] = ctx_.uptime_seconds();
    return JsonOk(resp, data);
}

// POST /api/v1/kick  body: {"user_id": 12345}
int AdminServer::HandleKickUser(HttpRequest* req, HttpResponse* resp) {
    hv::Json body;
    if (req->content_type == APPLICATION_JSON && !req->body.empty()) {
        body = nlohmann::json::parse(req->body, nullptr, false);
    }

    if (!body.contains("user_id") || !body["user_id"].is_number_integer()) {
        return JsonError(resp, ApiCode::kParamError, "missing or invalid user_id");
    }

    int64_t user_id = body["user_id"].get<int64_t>();
    auto conns = ConnManager::Instance().GetConns(user_id);

    if (conns.empty()) {
        return JsonError(resp, ApiCode::kNotFound, "user not online");
    }

    for (auto& c : conns) {
        c->Close();
    }

    NOVA_NLOG_INFO(kLogTag, "kicked user {} ({} connections)", user_id, conns.size());
    return JsonOk(resp, {{"kicked", static_cast<int>(conns.size())}});
}

} // namespace nova
