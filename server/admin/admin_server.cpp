#include "admin_server.h"
#include "http_helper.h"
#include "jwt_utils.h"
#include "password_utils.h"
#include "../core/server_context.h"
#include "../net/conn_manager.h"
#include "../core/logger.h"
#include "../dao/db_manager.h"
#include "../dao/user_dao_impl.h"
#include "../dao/audit_log_dao_impl.h"
#include "../dao/admin_session_dao_impl.h"
#include "../dao/rbac_dao_impl.h"
#include "../dao/message_dao_impl.h"

#include <hv/json.hpp>

#include <mbedtls/sha256.h>

#include <functional>
#include <optional>
#include <unordered_map>

namespace nova {

static constexpr const char* kLogTag = "Admin";
static constexpr size_t kAdminMaxBodySize = 1 * 1024 * 1024; // 1 MB

// SHA-256 哈希（用于 token_hash）
static std::string Sha256Hex(std::string_view data) {
    unsigned char hash[32];
    mbedtls_sha256(reinterpret_cast<const unsigned char*>(data.data()),
                   data.size(), hash, 0);
    char hex[65];
    for (int i = 0; i < 32; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    return std::string(hex, 64);
}

// 解析 JSON Body（统一入口，防止重复代码）
static std::optional<hv::Json> ParseJsonBody(HttpRequest* req) {
    if (req->content_type != APPLICATION_JSON || req->body.empty()) return std::nullopt;
    auto j = nlohmann::json::parse(req->body, nullptr, false);
    if (j.is_discarded()) return std::nullopt;
    return j;
}

AdminServer::AdminServer(ServerContext& ctx, DbManager& db)
    : ctx_(ctx), db_(db) {}

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
    server_->setThreadNum(1);
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

// ============================================================
// 路由注册
// ============================================================

void AdminServer::RegisterRoutes(const Options& opts) {
    if (!opts.jwt_secret.empty()) {
        service_.Use([this](HttpRequest* req, HttpResponse* resp) -> int {
            return AuthMiddleware(req, resp);
        });
    }

    // 健康检查
    service_.GET("/healthz", [this](auto* req, auto* resp) { return HandleHealthz(req, resp); });

    // 认证
    service_.POST("/api/v1/auth/login",  [this](auto* req, auto* resp) { return HandleLogin(req, resp); });
    service_.POST("/api/v1/auth/logout", [this](auto* req, auto* resp) { return HandleLogout(req, resp); });
    service_.GET("/api/v1/auth/me",      [this](auto* req, auto* resp) { return HandleMe(req, resp); });

    // 仪表盘
    service_.GET("/api/v1/dashboard/stats", [this](auto* req, auto* resp) { return HandleStats(req, resp); });

    // 用户管理
    service_.GET("/api/v1/users",    [this](auto* req, auto* resp) { return HandleListUsers(req, resp); });
    service_.POST("/api/v1/users",   [this](auto* req, auto* resp) { return HandleCreateUser(req, resp); });

    // libhv 路径参数: /api/v1/users/:id
    service_.GET("/api/v1/users/:id",    [this](auto* req, auto* resp) { return HandleGetUser(req, resp); });
    service_.Delete("/api/v1/users/:id", [this](auto* req, auto* resp) { return HandleDeleteUser(req, resp); });

    service_.POST("/api/v1/users/:id/reset-password", [this](auto* req, auto* resp) { return HandleResetPassword(req, resp); });
    service_.POST("/api/v1/users/:id/ban",   [this](auto* req, auto* resp) { return HandleBanUser(req, resp); });
    service_.POST("/api/v1/users/:id/unban", [this](auto* req, auto* resp) { return HandleUnbanUser(req, resp); });
    service_.POST("/api/v1/users/:id/kick",  [this](auto* req, auto* resp) { return HandleKickUser(req, resp); });

    // 消息管理
    service_.GET("/api/v1/messages",               [this](auto* req, auto* resp) { return HandleListMessages(req, resp); });
    service_.POST("/api/v1/messages/:id/recall",   [this](auto* req, auto* resp) { return HandleRecallMessage(req, resp); });

    // 审计日志
    service_.GET("/api/v1/audit-logs", [this](auto* req, auto* resp) { return HandleListAuditLogs(req, resp); });
}

// ============================================================
// 鉴权中间件
// ============================================================

int AdminServer::AuthMiddleware(HttpRequest* req, HttpResponse* resp) {
    // 清除客户端可能伪造的内部头
    req->SetHeader("X-Nova-User-Id", "");
    req->SetHeader("X-Nova-Permissions", "");

    // 免鉴权路径
    if (req->path == "/healthz" || req->path == "/api/v1/auth/login") {
        return HTTP_STATUS_NEXT;
    }

    // 请求体大小限制
    if (req->body.size() > kAdminMaxBodySize) {
        JsonError(resp, ApiCode::kParamError, "request body too large", 413);
        return 413;
    }

    std::string auth = req->GetHeader("Authorization");
    constexpr std::string_view kBearer = "Bearer ";
    if (auth.size() <= kBearer.size() || auth.substr(0, kBearer.size()) != kBearer) {
        JsonError(resp, ApiCode::kUnauthorized, "missing or invalid token", 401);
        return 401;
    }

    auto token_sv = std::string_view(auth).substr(kBearer.size());
    auto claims = JwtUtils::Verify(token_sv, opts_.jwt_secret);
    if (!claims) {
        JsonError(resp, ApiCode::kUnauthorized, "invalid or expired token", 401);
        return 401;
    }

    // 查 admin_sessions 黑名单
    AdminSessionDaoImpl session_dao(db_);
    std::string token_hash = Sha256Hex(token_sv);
    if (session_dao.IsRevoked(token_hash)) {
        JsonError(resp, ApiCode::kUnauthorized, "token has been revoked", 401);
        return 401;
    }

    // 注入 user_id
    req->SetHeader("X-Nova-User-Id", std::to_string(claims->user_id));

    // 注入 permissions（逗号分隔）
    RbacDaoImpl rbac_dao(db_);
    auto perms = rbac_dao.GetUserPermissions(claims->user_id);
    std::string perms_str;
    for (size_t i = 0; i < perms.size(); ++i) {
        if (i > 0) perms_str += ',';
        perms_str += perms[i];
    }
    req->SetHeader("X-Nova-Permissions", perms_str);

    return HTTP_STATUS_NEXT;
}

// ============================================================
// 工具函数
// ============================================================

std::string AdminServer::GetClientIp(HttpRequest* req) {
    // 优先 X-Forwarded-For，再 X-Real-IP，最后 peeraddr
    auto xff = req->GetHeader("X-Forwarded-For");
    if (!xff.empty()) {
        auto comma = xff.find(',');
        return comma != std::string::npos ? xff.substr(0, comma) : xff;
    }
    auto real_ip = req->GetHeader("X-Real-IP");
    if (!real_ip.empty()) return real_ip;
    return req->client_addr.ip;
}

void AdminServer::WriteAuditLog(int64_t user_id, const std::string& action,
                                const std::string& target_type, int64_t target_id,
                                const std::string& detail, const std::string& ip) {
    AuditLogDaoImpl dao(db_);
    AuditLog log;
    log.user_id     = user_id;
    log.action      = action;
    log.target_type = target_type;
    log.target_id   = target_id;
    log.detail      = detail;
    log.ip          = ip;
    if (!dao.Insert(log)) {
        NOVA_NLOG_WARN(kLogTag, "failed to write audit log: action={}", action);
    }
}

// ============================================================
// 健康检查
// ============================================================

int AdminServer::HandleHealthz(HttpRequest* /*req*/, HttpResponse* resp) {
    resp->content_type = APPLICATION_JSON;
    resp->body = R"({"status":"ok"})";
    return 200;
}

// ============================================================
// 认证
// ============================================================

int AdminServer::HandleLogin(HttpRequest* req, HttpResponse* resp) {
    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("uid") || !body_opt->contains("password")) {
        return JsonError(resp, ApiCode::kParamError, "uid and password required");
    }
    auto& body = *body_opt;

    std::string uid = body["uid"].get<std::string>();
    std::string password = body["password"].get<std::string>();

    UserDaoImpl user_dao(db_);
    auto user = user_dao.FindByUid(uid);
    if (!user) {
        return JsonError(resp, ApiCode::kUnauthorized, "invalid credentials", 401);
    }

    if (!PasswordUtils::Verify(password, user->password_hash)) {
        return JsonError(resp, ApiCode::kUnauthorized, "invalid credentials", 401);
    }

    if (user->status != 1) {
        return JsonError(resp, ApiCode::kForbidden, "account is disabled", 403);
    }

    // 检查是否有 admin.login 权限
    RbacDaoImpl rbac_dao(db_);
    if (!rbac_dao.HasPermission(user->id, "admin.login")) {
        return JsonError(resp, ApiCode::kForbidden, "no admin access", 403);
    }

    // 签发 JWT
    auto token = JwtUtils::Sign(user->id, opts_.jwt_secret, opts_.jwt_expires);
    if (token.empty()) {
        return JsonError(resp, ApiCode::kInternal, "failed to sign token");
    }

    // 记录 session（用于黑名单管理）
    AdminSessionDaoImpl session_dao(db_);
    AdminSession session;
    session.user_id    = user->id;
    session.token_hash = Sha256Hex(token);
    // 计算过期时间 (ISO-8601)
    auto now = std::time(nullptr);
    auto exp = now + opts_.jwt_expires;
    char exp_buf[32];
    struct tm tm_buf{};
    gmtime_s(&tm_buf, &exp);  // MSVC thread-safe variant
    std::strftime(exp_buf, sizeof(exp_buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    session.expires_at = exp_buf;
    session_dao.Insert(session);

    // 审计
    WriteAuditLog(user->id, "admin.login", "user", user->id, "{}", GetClientIp(req));

    return JsonOk(resp, {{"token", token}, {"expires_in", opts_.jwt_expires}});
}

int AdminServer::HandleLogout(HttpRequest* req, HttpResponse* resp) {
    int64_t user_id = GetCurrentUserId(req);

    // 吊销当前 token
    std::string auth = req->GetHeader("Authorization");
    constexpr std::string_view kBearer = "Bearer ";
    if (auth.size() <= kBearer.size()) {
        return JsonError(resp, ApiCode::kUnauthorized, "missing token", 401);
    }
    auto token_sv = std::string_view(auth).substr(kBearer.size());
    std::string token_hash = Sha256Hex(token_sv);

    AdminSessionDaoImpl session_dao(db_);
    session_dao.RevokeByTokenHash(token_hash);

    WriteAuditLog(user_id, "admin.logout", "user", user_id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleMe(HttpRequest* req, HttpResponse* resp) {
    int64_t user_id = GetCurrentUserId(req);
    if (user_id == 0) {
        return JsonError(resp, ApiCode::kUnauthorized, "not authenticated", 401);
    }

    UserDaoImpl user_dao(db_);
    auto user = user_dao.FindById(user_id);
    if (!user) {
        return JsonError(resp, ApiCode::kNotFound, "user not found");
    }

    RbacDaoImpl rbac_dao(db_);
    auto perms = rbac_dao.GetUserPermissions(user_id);

    nlohmann::json data;
    data["user_id"]     = user->id;
    data["uid"]         = user->uid;
    data["nickname"]    = user->nickname;
    data["permissions"] = perms;

    return JsonOk(resp, data);
}

// ============================================================
// 仪表盘
// ============================================================

int AdminServer::HandleStats(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "admin.dashboard");
    if (rc != 0) return rc;

    nlohmann::json data;
    data["connections"]    = ctx_.connection_count();
    data["online_users"]   = ctx_.online_user_count();
    data["messages_in"]    = ctx_.total_messages_in();
    data["messages_out"]   = ctx_.total_messages_out();
    data["bad_packets"]    = ctx_.bad_packets();
    data["uptime_seconds"] = ctx_.uptime_seconds();
    return JsonOk(resp, data);
}

// ============================================================
// 用户管理
// ============================================================

int AdminServer::HandleListUsers(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "user.view");
    if (rc != 0) return rc;

    auto pg = ParsePagination(req);
    std::string keyword = req->GetParam("keyword");
    int status = -1;  // -1 = no filter
    auto status_str = req->GetParam("status");
    if (!status_str.empty()) {
        int val = std::atoi(status_str.c_str());
        if (val > 0) status = val;  // 0 = all (same as no filter per API doc)
    }

    UserDaoImpl user_dao(db_);
    auto result = user_dao.ListUsers(keyword, status, pg.page, pg.page_size);

    nlohmann::json items = nlohmann::json::array();
    for (auto& u : result.items) {
        items.push_back({
            {"id",         u.id},
            {"uid",        u.uid},
            {"nickname",   u.nickname},
            {"avatar",     u.avatar},
            {"status",     u.status},
            {"is_online",  ConnManager::Instance().IsOnline(u.id)},
            {"created_at", u.created_at},
        });
    }

    return JsonOk(resp, PaginatedResult(items, result.total, pg));
}

int AdminServer::HandleCreateUser(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "user.create");
    if (rc != 0) return rc;

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("uid") || !body_opt->contains("password")) {
        return JsonError(resp, ApiCode::kParamError, "uid and password required");
    }
    auto& body = *body_opt;

    std::string uid = body["uid"].get<std::string>();
    std::string password = body["password"].get<std::string>();
    std::string nickname = body.value("nickname", uid);

    if (uid.empty() || password.empty()) {
        return JsonError(resp, ApiCode::kParamError, "uid and password cannot be empty");
    }

    // 检查 uid 是否已存在
    UserDaoImpl user_dao(db_);
    if (user_dao.FindByUid(uid)) {
        return JsonError(resp, ApiCode::kParamError, "uid already exists");
    }

    auto hash = PasswordUtils::Hash(password);
    if (hash.empty()) {
        return JsonError(resp, ApiCode::kInternal, "failed to hash password");
    }

    User user;
    user.uid           = uid;
    user.password_hash = hash;
    user.nickname      = nickname;

    if (!user_dao.Insert(user)) {
        return JsonError(resp, ApiCode::kInternal, "failed to create user");
    }

    int64_t admin_id = GetCurrentUserId(req);
    WriteAuditLog(admin_id, "user.create", "user", user.id,
                  nlohmann::json({{"uid", uid}}).dump(), GetClientIp(req));

    return JsonOk(resp, {{"id", user.id}, {"uid", uid}});
}

int AdminServer::HandleGetUser(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "user.view");
    if (rc != 0) return rc;

    auto id_str = req->GetParam("id");
    int64_t id = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, ApiCode::kParamError, "invalid user id");
    }

    UserDaoImpl user_dao(db_);
    auto user = user_dao.FindById(id);
    if (!user) {
        return JsonError(resp, ApiCode::kNotFound, "user not found");
    }

    nlohmann::json data;
    data["id"]         = user->id;
    data["uid"]        = user->uid;
    data["nickname"]   = user->nickname;
    data["avatar"]     = user->avatar;
    data["status"]     = user->status;
    data["is_online"]  = ConnManager::Instance().IsOnline(user->id);
    data["created_at"] = user->created_at;
    // devices: 查 user_devices 表
    auto devices = db_.DB().query_s<UserDevice>("user_id=?", id);
    nlohmann::json dev_arr = nlohmann::json::array();
    for (auto& d : devices) {
        dev_arr.push_back({
            {"device_id",      d.device_id},
            {"device_type",    d.device_type},
            {"last_active_at", d.last_active_at},
        });
    }
    data["devices"] = dev_arr;

    return JsonOk(resp, data);
}

int AdminServer::HandleDeleteUser(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "user.delete");
    if (rc != 0) return rc;

    auto id_str = req->GetParam("id");
    int64_t id = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, ApiCode::kParamError, "invalid user id");
    }

    UserDaoImpl user_dao(db_);
    if (!user_dao.FindById(id)) {
        return JsonError(resp, ApiCode::kNotFound, "user not found");
    }

    user_dao.SoftDelete(id);

    // 踢下线
    auto conns = ConnManager::Instance().GetConns(id);
    for (auto& c : conns) c->Close();

    int64_t admin_id = GetCurrentUserId(req);
    WriteAuditLog(admin_id, "user.delete", "user", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleResetPassword(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "user.edit");
    if (rc != 0) return rc;

    auto id_str = req->GetParam("id");
    int64_t id = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, ApiCode::kParamError, "invalid user id");
    }

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("new_password")) {
        return JsonError(resp, ApiCode::kParamError, "new_password required");
    }

    std::string new_password = (*body_opt)["new_password"].get<std::string>();
    if (new_password.empty()) {
        return JsonError(resp, ApiCode::kParamError, "new_password cannot be empty");
    }

    auto hash = PasswordUtils::Hash(new_password);
    if (hash.empty()) {
        return JsonError(resp, ApiCode::kInternal, "failed to hash password");
    }

    UserDaoImpl user_dao(db_);
    if (!user_dao.UpdatePassword(id, hash)) {
        return JsonError(resp, ApiCode::kNotFound, "user not found");
    }

    int64_t admin_id = GetCurrentUserId(req);
    WriteAuditLog(admin_id, "user.reset_password", "user", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleBanUser(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "user.ban");
    if (rc != 0) return rc;

    auto id_str = req->GetParam("id");
    int64_t id = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, ApiCode::kParamError, "invalid user id");
    }

    auto body_opt = ParseJsonBody(req);
    std::string reason = body_opt ? body_opt->value("reason", "") : "";

    UserDaoImpl user_dao(db_);
    if (!user_dao.UpdateStatus(id, 2)) {
        return JsonError(resp, ApiCode::kNotFound, "user not found");
    }

    // 踢下线
    auto conns = ConnManager::Instance().GetConns(id);
    for (auto& c : conns) c->Close();

    int64_t admin_id = GetCurrentUserId(req);
    WriteAuditLog(admin_id, "user.ban", "user", id,
                  nlohmann::json({{"reason", reason}}).dump(), GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleUnbanUser(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "user.ban");
    if (rc != 0) return rc;

    auto id_str = req->GetParam("id");
    int64_t id = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, ApiCode::kParamError, "invalid user id");
    }

    UserDaoImpl user_dao(db_);
    if (!user_dao.UpdateStatus(id, 1)) {
        return JsonError(resp, ApiCode::kNotFound, "user not found");
    }

    int64_t admin_id = GetCurrentUserId(req);
    WriteAuditLog(admin_id, "user.unban", "user", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleKickUser(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "user.ban");
    if (rc != 0) return rc;

    auto id_str = req->GetParam("id");
    int64_t id = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, ApiCode::kParamError, "invalid user id");
    }

    auto conns = ConnManager::Instance().GetConns(id);
    if (conns.empty()) {
        return JsonError(resp, ApiCode::kNotFound, "user not online");
    }

    for (auto& c : conns) c->Close();

    int64_t admin_id = GetCurrentUserId(req);
    WriteAuditLog(admin_id, "user.kick", "user", id, "{}", GetClientIp(req));

    NOVA_NLOG_INFO(kLogTag, "kicked user {} ({} connections)", id, conns.size());
    return JsonOk(resp, {{"kicked", static_cast<int>(conns.size())}});
}

// ============================================================
// 消息管理
// ============================================================

int AdminServer::HandleListMessages(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "msg.delete_all");
    if (rc != 0) return rc;

    auto pg = ParsePagination(req);
    int64_t conversation_id = 0;
    auto cid_str = req->GetParam("conversation_id");
    if (!cid_str.empty()) conversation_id = std::atoll(cid_str.c_str());

    std::string start_time = req->GetParam("start_time");
    std::string end_time   = req->GetParam("end_time");

    MessageDaoImpl msg_dao(db_);
    auto result = msg_dao.ListMessages(conversation_id, start_time, end_time, pg.page, pg.page_size);

    // sender_uid 缓存，避免 N+1 查询
    UserDaoImpl user_dao(db_);
    std::unordered_map<int64_t, std::string> uid_cache;
    auto resolve_uid = [&](int64_t id) -> const std::string& {
        auto [it, inserted] = uid_cache.try_emplace(id);
        if (inserted) {
            auto u = user_dao.FindById(id);
            it->second = u ? u->uid : "";
        }
        return it->second;
    };

    nlohmann::json items = nlohmann::json::array();
    for (auto& m : result.items) {
        nlohmann::json item;
        item["id"]              = m.id;
        item["conversation_id"] = m.conversation_id;
        item["sender_id"]       = m.sender_id;
        item["sender_uid"]      = resolve_uid(m.sender_id);
        item["seq"]             = m.seq;
        item["msg_type"]        = m.msg_type;
        item["content"]         = m.content;
        item["status"]          = m.status;
        item["created_at"]      = m.created_at;
        items.push_back(item);
    }

    return JsonOk(resp, PaginatedResult(items, result.total, pg));
}

int AdminServer::HandleRecallMessage(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "msg.delete_all");
    if (rc != 0) return rc;

    auto id_str = req->GetParam("id");
    int64_t id = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, ApiCode::kParamError, "invalid message id");
    }

    auto body_opt = ParseJsonBody(req);
    std::string reason = body_opt ? body_opt->value("reason", "") : "";

    MessageDaoImpl msg_dao(db_);
    auto msg = msg_dao.FindById(id);
    if (!msg) {
        return JsonError(resp, ApiCode::kNotFound, "message not found");
    }

    if (!msg_dao.UpdateStatus(id, 1)) {
        return JsonError(resp, ApiCode::kInternal, "failed to recall message");
    }

    int64_t admin_id = GetCurrentUserId(req);
    WriteAuditLog(admin_id, "msg.recall", "message", id,
                  nlohmann::json({{"reason", reason}, {"conversation_id", msg->conversation_id}}).dump(),
                  GetClientIp(req));

    return JsonOk(resp);
}

// ============================================================
// 审计日志
// ============================================================

int AdminServer::HandleListAuditLogs(HttpRequest* req, HttpResponse* resp) {
    int rc = RequirePermission(req, resp, "admin.dashboard");
    if (rc != 0) return rc;

    auto pg = ParsePagination(req);
    int64_t user_id = 0;
    auto uid_str = req->GetParam("user_id");
    if (!uid_str.empty()) user_id = std::atoll(uid_str.c_str());

    std::string action     = req->GetParam("action");
    std::string start_time = req->GetParam("start_time");
    std::string end_time   = req->GetParam("end_time");

    AuditLogDaoImpl audit_dao(db_);
    auto result = audit_dao.List(user_id, action, start_time, end_time, pg.page, pg.page_size);

    // operator_uid 缓存，避免 N+1 查询
    UserDaoImpl user_dao(db_);
    std::unordered_map<int64_t, std::string> uid_cache;
    auto resolve_uid = [&](int64_t id) -> const std::string& {
        auto [it, inserted] = uid_cache.try_emplace(id);
        if (inserted) {
            auto u = user_dao.FindById(id);
            it->second = u ? u->uid : "";
        }
        return it->second;
    };

    nlohmann::json items = nlohmann::json::array();
    for (auto& log : result.items) {
        nlohmann::json item;
        item["id"]           = log.id;
        item["user_id"]      = log.user_id;
        item["operator_uid"] = resolve_uid(log.user_id);
        item["action"]       = log.action;
        item["target_type"]  = log.target_type;
        item["target_id"]    = log.target_id;
        // detail 是 JSON 字符串，解析后嵌入
        item["detail"] = nlohmann::json::parse(log.detail, nullptr, false);
        if (item["detail"].is_discarded()) item["detail"] = nlohmann::json::object();
        item["ip"]         = log.ip;
        item["created_at"] = log.created_at;
        items.push_back(item);
    }

    return JsonOk(resp, PaginatedResult(items, result.total, pg));
}

} // namespace nova
