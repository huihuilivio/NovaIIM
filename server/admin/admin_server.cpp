#include "admin_server.h"
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
    // 鉴权中间件（token 非空时启用）
    if (!opts.token.empty()) {
        std::string token = opts.token;  // 拷贝到 lambda
        service_.Use([token](HttpRequest* req, HttpResponse* resp) -> int {
            return AuthMiddleware(req, resp, token);
        });
    }

    service_.GET("/healthz", [this](HttpRequest* req, HttpResponse* resp) {
        return HandleHealthz(req, resp);
    });

    service_.GET("/api/v1/stats", [this](HttpRequest* req, HttpResponse* resp) {
        return HandleStats(req, resp);
    });

    service_.POST("/api/v1/kick", [this](HttpRequest* req, HttpResponse* resp) {
        return HandleKickUser(req, resp);
    });
}

int AdminServer::AuthMiddleware(HttpRequest* req, HttpResponse* resp, const std::string& token) {
    // /healthz 不需要鉴权
    if (req->path == "/healthz") {
        return HTTP_STATUS_NEXT;
    }

    std::string auth = req->GetHeader("Authorization");
    std::string expected = "Bearer " + token;
    if (auth != expected) {
        resp->SetHeader("Content-Type", "application/json");
        resp->json["error"] = "unauthorized";
        return 401;
    }
    return HTTP_STATUS_NEXT;
}

// GET /healthz — 健康检查（用于负载均衡/K8s 探针）
int AdminServer::HandleHealthz(HttpRequest* /*req*/, HttpResponse* resp) {
    resp->SetHeader("Content-Type", "application/json");
    resp->json["status"] = "ok";
    return 200;
}

// GET /api/v1/stats — 运行时指标
int AdminServer::HandleStats(HttpRequest* /*req*/, HttpResponse* resp) {
    resp->SetHeader("Content-Type", "application/json");
    resp->json["connections"]   = ctx_.connection_count();
    resp->json["online_users"]  = ctx_.online_user_count();
    resp->json["messages_in"]   = ctx_.total_messages_in();
    resp->json["messages_out"]  = ctx_.total_messages_out();
    resp->json["bad_packets"]   = ctx_.bad_packets();
    resp->json["uptime_seconds"] = ctx_.uptime_seconds();
    return 200;
}

// POST /api/v1/kick  body: {"user_id": 12345}
int AdminServer::HandleKickUser(HttpRequest* req, HttpResponse* resp) {
    resp->SetHeader("Content-Type", "application/json");

    if (!req->json.contains("user_id") || !req->json["user_id"].is_number_integer()) {
        resp->json["error"] = "missing or invalid user_id";
        return 400;
    }

    int64_t user_id = req->json["user_id"].get<int64_t>();
    auto conns = ConnManager::Instance().GetConns(user_id);

    if (conns.empty()) {
        resp->json["error"] = "user not online";
        return 404;
    }

    for (auto& c : conns) {
        c->Close();
    }

    NOVA_NLOG_INFO(kLogTag, "kicked user {} ({} connections)", user_id, conns.size());
    resp->json["kicked"] = static_cast<int>(conns.size());
    return 200;
}

} // namespace nova
