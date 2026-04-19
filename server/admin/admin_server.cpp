#include "admin_server.h"
#include "http_helper.h"
#include "jwt_utils.h"
#include "password_utils.h"
#include "../core/server_context.h"
#include "../core/logger.h"
#include "../dao/dao_factory.h"
#include "../dao/user_dao.h"
#include "../dao/message_dao.h"
#include "../dao/conversation_dao.h"
#include "../dao/audit_log_dao.h"
#include "../dao/admin_session_dao.h"
#include "../dao/admin_account_dao.h"
#include "../dao/rbac_dao.h"

#include <nova/protocol.h>
#include <hv/json.hpp>

#include <mbedtls/sha256.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <unordered_map>

namespace nova {

static constexpr const char* kLogTag      = "Admin";
static constexpr size_t kAdminMaxBodySize = 1 * 1024 * 1024;  // 1 MB

// 邮箱格式校验（与 UserService 一致）
static bool IsValidEmail(const std::string& email) {
    auto at = email.find('@');
    if (at == std::string::npos || at == 0 || at == email.size() - 1)
        return false;
    auto dot = email.find('.', at + 1);
    if (dot == std::string::npos || dot == at + 1 || dot == email.size() - 1)
        return false;
    for (unsigned char c : email) {
        if (c <= 0x20 || c == 0x7F)
            return false;
    }
    return true;
}

// 首尾空白裁剪（与 UserService 一致）
static void TrimInPlace(std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end   = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) {
        s.clear();
    } else {
        s = s.substr(start, end - start + 1);
    }
}

// 邮箱转小写
static void EmailToLower(std::string& email) {
    std::transform(email.begin(), email.end(), email.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

// SHA-256 哈希（用于 token_hash）
static std::string Sha256Hex(std::string_view data) {
    unsigned char hash[32];
    mbedtls_sha256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash, 0);
    char hex[65];
    for (int i = 0; i < 32; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    return std::string(hex, 64);
}

// 解析 JSON Body（统一入口，防止重复代码）
static std::optional<hv::Json> ParseJsonBody(HttpRequest* req) {
    if (req->content_type != APPLICATION_JSON || req->body.empty())
        return std::nullopt;
    auto j = nlohmann::json::parse(req->body, nullptr, false);
    if (j.is_discarded())
        return std::nullopt;
    return j;
}

AdminServer::AdminServer(ServerContext& ctx)
    : ctx_(ctx),
      login_limiter_(ctx.config().admin.login_max_attempts, std::chrono::seconds(ctx.config().admin.login_window_secs)) {
}

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
        service_.Use([this](HttpRequest* req, HttpResponse* resp) -> int { return AuthMiddleware(req, resp); });
    }

    // 健康检查
    service_.GET("/healthz", [this](auto* req, auto* resp) { return HandleHealthz(req, resp); });

    // 认证
    service_.POST("/api/v1/auth/login", [this](auto* req, auto* resp) { return HandleLogin(req, resp); });
    service_.POST("/api/v1/auth/logout", [this](auto* req, auto* resp) { return HandleLogout(req, resp); });
    service_.GET("/api/v1/auth/me", [this](auto* req, auto* resp) { return HandleMe(req, resp); });

    // 仪表盘
    service_.GET("/api/v1/dashboard/stats", [this](auto* req, auto* resp) { return HandleStats(req, resp); });

    // 用户管理
    service_.GET("/api/v1/users", [this](auto* req, auto* resp) { return HandleListUsers(req, resp); });
    service_.POST("/api/v1/users", [this](auto* req, auto* resp) { return HandleCreateUser(req, resp); });

    // libhv 路径参数: /api/v1/users/:id
    service_.GET("/api/v1/users/:id", [this](auto* req, auto* resp) { return HandleGetUser(req, resp); });
    service_.Delete("/api/v1/users/:id", [this](auto* req, auto* resp) { return HandleDeleteUser(req, resp); });

    service_.POST("/api/v1/users/:id/reset-password",
                  [this](auto* req, auto* resp) { return HandleResetPassword(req, resp); });
    service_.POST("/api/v1/users/:id/ban", [this](auto* req, auto* resp) { return HandleBanUser(req, resp); });
    service_.POST("/api/v1/users/:id/unban", [this](auto* req, auto* resp) { return HandleUnbanUser(req, resp); });
    service_.POST("/api/v1/users/:id/kick", [this](auto* req, auto* resp) { return HandleKickUser(req, resp); });

    // 消息管理
    service_.GET("/api/v1/messages", [this](auto* req, auto* resp) { return HandleListMessages(req, resp); });
    service_.POST("/api/v1/messages/:id/recall",
                  [this](auto* req, auto* resp) { return HandleRecallMessage(req, resp); });

    // 审计日志
    service_.GET("/api/v1/audit-logs", [this](auto* req, auto* resp) { return HandleListAuditLogs(req, resp); });
}

// ============================================================
// 鉴权中间件
// ============================================================

int AdminServer::AuthMiddleware(HttpRequest* req, HttpResponse* resp) {
    // 清除客户端可能伪造的内部头
    req->SetHeader("X-Nova-Admin-Id", "");
    req->SetHeader("X-Nova-Permissions", "");

    // 免鉴权路径
    if (req->path == "/healthz" || req->path == "/api/v1/auth/login") {
        return HTTP_STATUS_NEXT;
    }

    // 请求体大小限制
    if (req->body.size() > kAdminMaxBodySize) {
        JsonError(resp, api_err::kBodyTooLarge);
        return 413;
    }

    std::string auth                   = req->GetHeader("Authorization");
    constexpr std::string_view kBearer = "Bearer ";
    if (auth.size() <= kBearer.size() || auth.substr(0, kBearer.size()) != kBearer) {
        JsonError(resp, api_err::kMissingToken);
        return 401;
    }

    auto token_sv = std::string_view(auth).substr(kBearer.size());
    auto claims   = JwtUtils::Verify(token_sv, opts_.jwt_secret);
    if (!claims) {
        JsonError(resp, api_err::kTokenExpired);
        return 401;
    }

    // 查 admin_sessions 黑名单
    std::string token_hash = Sha256Hex(token_sv);
    if (ctx_.dao().AdminSession().IsRevoked(token_hash)) {
        JsonError(resp, api_err::kTokenRevoked);
        return 401;
    }

    // 注入 admin_id
    req->SetHeader("X-Nova-Admin-Id", std::to_string(claims->admin_id));

    // 检查管理员账户状态（禁止被封禁/删除的管理员使用有效 JWT）
    auto admin_account = ctx_.dao().AdminAccount().FindById(claims->admin_id);
    if (!admin_account || admin_account->status == static_cast<int>(AccountStatus::Banned) ||
        admin_account->status == static_cast<int>(AccountStatus::Deleted)) {
        JsonError(resp, api_err::kAccountDisabled);
        return 403;
    }

    // 注入 permissions（逗号分隔）
    auto perms = ctx_.dao().Rbac().GetUserPermissions(claims->admin_id);
    std::string perms_str;
    for (size_t i = 0; i < perms.size(); ++i) {
        if (i > 0)
            perms_str += ',';
        perms_str += perms[i];
    }
    req->SetHeader("X-Nova-Permissions", perms_str);

    return HTTP_STATUS_NEXT;
}

// ============================================================
// 工具函数
// ============================================================

std::string AdminServer::GetClientIp(HttpRequest* req) const {
    // 仅在 trust_proxy 启用时信任代理注入的 IP 头
    // 默认不信任，防止客户端伪造 IP 写入审计日志
    if (ctx_.config().admin.trust_proxy) {
        auto xff = req->GetHeader("X-Forwarded-For");
        if (!xff.empty()) {
            auto comma = xff.find(',');
            std::string ip = comma != std::string::npos ? xff.substr(0, comma) : xff;
            // 裁剪空白并限制长度，防止恶意内容写入审计日志
            auto start = ip.find_first_not_of(" \t");
            auto end   = ip.find_last_not_of(" \t");
            if (start != std::string::npos) {
                ip = ip.substr(start, end - start + 1);
                if (!ip.empty() && ip.size() <= 45)  // max IPv6 length
                    return ip;
            }
        }
        auto real_ip = req->GetHeader("X-Real-IP");
        if (!real_ip.empty() && real_ip.size() <= 45)
            return real_ip;
    }
    return req->client_addr.ip;
}

void AdminServer::WriteAuditLog(int64_t admin_id, const std::string& action, const std::string& target_type,
                                int64_t target_id, const std::string& detail, const std::string& ip) {
    AuditLog log;
    log.admin_id    = admin_id;
    log.action      = action;
    log.target_type = target_type;
    log.target_id   = target_id;
    log.detail      = detail;
    log.ip_address  = ip;
    if (!ctx_.dao().AuditLog().Insert(log)) {
        NOVA_NLOG_WARN(kLogTag, "failed to write audit log: action={}", action);
    }
}

// ============================================================
// 健康检查
// ============================================================

int AdminServer::HandleHealthz(HttpRequest* /*req*/, HttpResponse* resp) {
    resp->content_type = APPLICATION_JSON;
    resp->body         = R"({"status":"ok"})";
    return 200;
}

// ============================================================
// 认证
// ============================================================

int AdminServer::HandleLogin(HttpRequest* req, HttpResponse* resp) {
    auto dao_session = ctx_.dao().Session();

    // body 大小限制（middleware 对 login 路径免鉴权但同样需要限制）
    if (req->body.size() > kAdminMaxBodySize) {
        return JsonError(resp, api_err::kBodyTooLarge);
    }

    // 频率限制（按客户端 IP）
    std::string client_ip = GetClientIp(req);
    if (!login_limiter_.Allow(client_ip)) {
        return JsonError(resp, api_err::kRateLimited);
    }

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("uid") || !body_opt->contains("password")) {
        return JsonError(resp, api_err::kUidPasswordRequired);
    }
    auto& body = *body_opt;

    if (!body["uid"].is_string() || !body["password"].is_string()) {
        return JsonError(resp, api_err::kUidPasswordStrings);
    }

    std::string uid      = body["uid"].get<std::string>();
    std::string password = body["password"].get<std::string>();

    auto admin = ctx_.dao().AdminAccount().FindByUid(uid);
    if (!admin) {
        login_limiter_.RecordFailure(client_ip);
        // 安全：清除内存中的明文密码
        volatile char* p = reinterpret_cast<volatile char*>(password.data());
        for (size_t i = 0; i < password.size(); ++i)
            p[i] = 0;
        password.clear();
        return JsonError(resp, api_err::kInvalidCredentials);
    }

    if (!PasswordUtils::Verify(password, admin->password_hash)) {
        login_limiter_.RecordFailure(client_ip);
        // 安全：清除内存中的明文密码
        volatile char* p = reinterpret_cast<volatile char*>(password.data());
        for (size_t i = 0; i < password.size(); ++i)
            p[i] = 0;
        password.clear();
        return JsonError(resp, api_err::kInvalidCredentials);
    }

    // 安全：验证完成后立即清除明文密码
    {
        volatile char* p = reinterpret_cast<volatile char*>(password.data());
        for (size_t i = 0; i < password.size(); ++i)
            p[i] = 0;
        password.clear();
    }

    if (admin->status != static_cast<int>(AccountStatus::Normal)) {
        return JsonError(resp, api_err::kAccountDisabled);
    }

    // 检查是否有 admin.login 权限
    if (!ctx_.dao().Rbac().HasPermission(admin->id, "admin.login")) {
        return JsonError(resp, api_err::kNoAdminAccess);
    }

    // 签发 JWT
    login_limiter_.Reset(client_ip);
    auto token = JwtUtils::Sign(admin->id, opts_.jwt_secret, opts_.jwt_expires);
    if (token.empty()) {
        return JsonError(resp, api_err::kSignTokenFailed);
    }

    // 记录 session（用于黑名单管理）
    AdminSession session;
    session.admin_id   = admin->id;
    session.token_hash = Sha256Hex(token);
    // 计算过期时间 (ISO-8601)
    auto now = std::time(nullptr);
    auto exp = now + opts_.jwt_expires;
    char exp_buf[32];
    struct tm tm_buf {};
#ifdef _MSC_VER
    gmtime_s(&tm_buf, &exp);
#else
    gmtime_r(&exp, &tm_buf);
#endif
    std::strftime(exp_buf, sizeof(exp_buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    session.expires_at = exp_buf;
    if (!ctx_.dao().AdminSession().Insert(session)) {
        NOVA_NLOG_ERROR(kLogTag, "failed to persist session for admin_id={}, rejecting login", admin->id);
        return JsonError(resp, api_err::kDatabaseError);
    }

    // 审计
    WriteAuditLog(admin->id, "admin.login", "admin", admin->id, "{}", GetClientIp(req));

    return JsonOk(resp, {{"token", token}, {"expires_in", opts_.jwt_expires}});
}

int AdminServer::HandleLogout(HttpRequest* req, HttpResponse* resp) {
    auto session     = ctx_.dao().Session();
    int64_t admin_id = GetCurrentAdminId(req);

    // 吊销当前 token
    std::string auth                   = req->GetHeader("Authorization");
    constexpr std::string_view kBearer = "Bearer ";
    if (auth.size() <= kBearer.size()) {
        return JsonError(resp, api_err::kMissingToken);
    }
    auto token_sv          = std::string_view(auth).substr(kBearer.size());
    std::string token_hash = Sha256Hex(token_sv);

    ctx_.dao().AdminSession().RevokeByTokenHash(token_hash);

    WriteAuditLog(admin_id, "admin.logout", "admin", admin_id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleMe(HttpRequest* req, HttpResponse* resp) {
    auto session     = ctx_.dao().Session();
    int64_t admin_id = GetCurrentAdminId(req);
    if (admin_id == 0) {
        return JsonError(resp, api_err::kNotAuthenticated);
    }

    auto admin = ctx_.dao().AdminAccount().FindById(admin_id);
    if (!admin) {
        return JsonError(resp, api_err::kAdminNotFound);
    }

    auto perms = ctx_.dao().Rbac().GetUserPermissions(admin_id);

    nlohmann::json data;
    data["admin_id"]    = admin->id;
    data["uid"]         = admin->uid;
    data["nickname"]    = admin->nickname;
    data["permissions"] = perms;

    return JsonOk(resp, data);
}

// ============================================================
// 仪表盘
// ============================================================

int AdminServer::HandleStats(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc = RequirePermission(req, resp, "admin.dashboard");
    if (rc != 0)
        return rc;

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
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "user.view");
    if (rc != 0)
        return rc;

    auto pg             = ParsePagination(req);
    std::string keyword = req->GetParam("keyword");
    int status          = -1;  // -1 = no filter
    auto status_str     = req->GetParam("status");
    if (!status_str.empty()) {
        int val = std::atoi(status_str.c_str());
        if (val > 0)
            status = val;  // 0 = all (same as no filter per API doc)
    }

    auto result = ctx_.dao().User().ListUsers(keyword, status, pg.page, pg.page_size);

    nlohmann::json items = nlohmann::json::array();
    for (auto& u : result.items) {
        items.push_back({
            {"uid", u.uid},
            {"email", u.email},
            {"nickname", u.nickname},
            {"avatar", u.avatar},
            {"status", u.status},
            {"is_online", ctx_.conn_manager().IsOnline(u.id)},
            {"created_at", u.created_at},
        });
    }

    return JsonOk(resp, PaginatedResult(items, result.total, pg));
}

int AdminServer::HandleCreateUser(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "user.create");
    if (rc != 0)
        return rc;

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("email") || !body_opt->contains("password")) {
        return JsonError(resp, api_err::kEmailPasswordRequired);
    }
    auto& body = *body_opt;

    if (!body["email"].is_string() || !body["password"].is_string()) {
        return JsonError(resp, api_err::kEmailPasswordStrings);
    }

    std::string email    = body["email"].get<std::string>();
    std::string password = body["password"].get<std::string>();
    std::string nickname = body.value("nickname", email);

    if (email.empty() || password.empty()) {
        return JsonError(resp, api_err::kEmailPasswordEmpty);
    }

    // 昵称校验（trim + 长度 + 控制字符）
    TrimInPlace(nickname);
    if (nickname.size() > 100) {
        return JsonError(resp, api_err::kNicknameTooLong);
    }
    if (std::any_of(nickname.begin(), nickname.end(),
                    [](unsigned char c) { return c < 0x20 || c == 0x7F; })) {
        return JsonError(resp, api_err::kNicknameInvalid);
    }

    // 邮箱校验（trim + lowercase + 格式 + 长度）
    TrimInPlace(email);
    EmailToLower(email);
    if (email.size() > 255) {
        return JsonError(resp, api_err::kEmailTooLong);
    }
    if (!IsValidEmail(email)) {
        return JsonError(resp, api_err::kEmailInvalidFormat);
    }

    // 密码长度校验
    if (password.size() < 6) {
        return JsonError(resp, api_err::kPasswordTooShort);
    }
    if (password.size() > 128) {
        return JsonError(resp, api_err::kPasswordTooLong);
    }

    // 检查 email 是否已存在
    if (ctx_.dao().User().FindByEmail(email)) {
        return JsonError(resp, api_err::kEmailAlreadyExists);
    }

    auto hash = PasswordUtils::Hash(password);
    // 安全：哈希完成后立即清除明文密码
    {
        volatile char* p = reinterpret_cast<volatile char*>(password.data());
        for (size_t i = 0; i < password.size(); ++i)
            p[i] = 0;
        password.clear();
    }
    if (hash.empty()) {
        return JsonError(resp, api_err::kHashFailed);
    }

    User user;
    user.uid           = ctx_.snowflake().NextIdStr();
    user.email         = email;
    user.password_hash = hash;
    user.nickname      = nickname;

    if (!ctx_.dao().User().Insert(user)) {
        // 区分 UNIQUE 冲突（并发创建同一邮箱）与其他 DB 错误
        if (ctx_.dao().User().FindByEmail(email)) {
            return JsonError(resp, api_err::kEmailAlreadyExists);
        }
        return JsonError(resp, api_err::kCreateUserFailed);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "user.create", "user", user.id, nlohmann::json({{"email", email}}).dump(), GetClientIp(req));

    return JsonOk(resp, {{"uid", user.uid}, {"email", email}});
}

int AdminServer::HandleGetUser(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "user.view");
    if (rc != 0)
        return rc;

    auto uid_str = req->GetParam("id");
    if (uid_str.empty()) {
        return JsonError(resp, api_err::kInvalidUserId);
    }

    auto user = ctx_.dao().User().FindByUid(uid_str);
    if (!user) {
        return JsonError(resp, api_err::kUserNotFound);
    }

    nlohmann::json data;
    data["uid"]        = user->uid;
    data["email"]      = user->email;
    data["nickname"]   = user->nickname;
    data["avatar"]     = user->avatar;
    data["status"]     = user->status;
    data["is_online"]  = ctx_.conn_manager().IsOnline(user->id);
    data["created_at"] = user->created_at;
    // devices: 查 user_devices 表
    auto devices           = ctx_.dao().User().ListDevicesByUser(user->uid);
    nlohmann::json dev_arr = nlohmann::json::array();
    for (auto& d : devices) {
        dev_arr.push_back({
            {"device_id", d.device_id},
            {"device_type", d.device_type},
            {"last_active_at", d.last_active_at},
        });
    }
    data["devices"] = dev_arr;

    return JsonOk(resp, data);
}

int AdminServer::HandleDeleteUser(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "user.delete");
    if (rc != 0)
        return rc;

    auto uid_str = req->GetParam("id");
    if (uid_str.empty()) {
        return JsonError(resp, api_err::kInvalidUserId);
    }

    auto id_opt = ctx_.dao().User().SoftDelete(uid_str);
    if (!id_opt) {
        return JsonError(resp, api_err::kUserNotFound);
    }
    int64_t id = *id_opt;

    // 踢下线
    auto conns = ctx_.conn_manager().GetConns(id);
    for (auto& c : conns)
        c->Close();

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "user.delete", "user", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleResetPassword(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "user.edit");
    if (rc != 0)
        return rc;

    auto uid_str = req->GetParam("id");
    if (uid_str.empty()) {
        return JsonError(resp, api_err::kInvalidUserId);
    }

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("new_password")) {
        return JsonError(resp, api_err::kNewPasswordRequired);
    }

    if (!(*body_opt)["new_password"].is_string()) {
        return JsonError(resp, api_err::kNewPasswordString);
    }

    std::string new_password = (*body_opt)["new_password"].get<std::string>();
    if (new_password.empty()) {
        return JsonError(resp, api_err::kNewPasswordEmpty);
    }
    if (new_password.size() < 6) {
        return JsonError(resp, api_err::kPasswordTooShort);
    }
    if (new_password.size() > 128) {
        return JsonError(resp, api_err::kPasswordTooLong);
    }

    auto hash = PasswordUtils::Hash(new_password);
    // 安全：哈希完成后立即清除明文密码
    {
        volatile char* p = reinterpret_cast<volatile char*>(new_password.data());
        for (size_t i = 0; i < new_password.size(); ++i)
            p[i] = 0;
        new_password.clear();
    }
    if (hash.empty()) {
        return JsonError(resp, api_err::kHashFailed);
    }

    auto id_opt = ctx_.dao().User().UpdatePassword(uid_str, hash);
    if (!id_opt) {
        return JsonError(resp, api_err::kUserNotFound);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "user.reset_password", "user", *id_opt, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleBanUser(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "user.ban");
    if (rc != 0)
        return rc;

    auto uid_str = req->GetParam("id");
    if (uid_str.empty()) {
        return JsonError(resp, api_err::kInvalidUserId);
    }

    auto body_opt      = ParseJsonBody(req);
    std::string reason = body_opt ? body_opt->value("reason", "") : "";

    auto id_opt = ctx_.dao().User().UpdateStatus(uid_str, static_cast<int>(AccountStatus::Banned));
    if (!id_opt) {
        return JsonError(resp, api_err::kUserNotFound);
    }
    int64_t id = *id_opt;

    // 踢下线
    auto conns = ctx_.conn_manager().GetConns(id);
    for (auto& c : conns)
        c->Close();

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "user.ban", "user", id, nlohmann::json({{"reason", reason}}).dump(), GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleUnbanUser(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "user.ban");
    if (rc != 0)
        return rc;

    auto uid_str = req->GetParam("id");
    if (uid_str.empty()) {
        return JsonError(resp, api_err::kInvalidUserId);
    }

    auto id_opt = ctx_.dao().User().UpdateStatus(uid_str, static_cast<int>(AccountStatus::Normal));
    if (!id_opt) {
        return JsonError(resp, api_err::kUserNotFound);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "user.unban", "user", *id_opt, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleKickUser(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc = RequirePermission(req, resp, "user.ban");
    if (rc != 0)
        return rc;

    auto uid_str = req->GetParam("id");
    if (uid_str.empty()) {
        return JsonError(resp, api_err::kInvalidUserId);
    }

    auto user_opt = ctx_.dao().User().FindByUid(uid_str);
    if (!user_opt) {
        return JsonError(resp, api_err::kUserNotFound);
    }
    int64_t id = user_opt->id;

    auto conns = ctx_.conn_manager().GetConns(id);
    if (conns.empty()) {
        return JsonError(resp, api_err::kUserNotOnline);
    }

    for (auto& c : conns)
        c->Close();

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "user.kick", "user", id, "{}", GetClientIp(req));

    NOVA_NLOG_INFO(kLogTag, "kicked user {} ({} connections)", id, conns.size());
    return JsonOk(resp, {{"kicked", static_cast<int>(conns.size())}});
}

// ============================================================
// 消息管理
// ============================================================

int AdminServer::HandleListMessages(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "msg.delete_all");
    if (rc != 0)
        return rc;

    auto pg                 = ParsePagination(req);
    int64_t conversation_id = 0;
    auto cid_str            = req->GetParam("conversation_id");
    if (!cid_str.empty())
        conversation_id = std::atoll(cid_str.c_str());

    std::string start_time = req->GetParam("start_time");
    std::string end_time   = req->GetParam("end_time");

    auto result = ctx_.dao().Message().ListMessages(conversation_id, start_time, end_time, pg.page, pg.page_size);

    // 批量查询所有 sender uid（避免 N+1）
    std::vector<int64_t> sender_ids;
    sender_ids.reserve(result.items.size());
    for (const auto& m : result.items) {
        sender_ids.push_back(m.sender_id);
    }
    auto sender_users = ctx_.dao().User().FindByIds(sender_ids);
    std::unordered_map<int64_t, std::string> uid_cache;
    for (const auto& u : sender_users) {
        uid_cache[u.id] = u.uid;
    }
    auto resolve_uid = [&](int64_t id) -> std::string {
        auto it = uid_cache.find(id);
        return it != uid_cache.end() ? it->second : "[deleted]";
    };

    nlohmann::json items = nlohmann::json::array();
    for (auto& m : result.items) {
        nlohmann::json item;
        item["id"]              = m.id;
        item["conversation_id"] = m.conversation_id;
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
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "msg.delete_all");
    if (rc != 0)
        return rc;

    auto id_str = req->GetParam("id");
    int64_t id  = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, api_err::kInvalidMessageId);
    }

    auto body_opt      = ParseJsonBody(req);
    std::string reason = body_opt ? body_opt->value("reason", "") : "";

    auto msg = ctx_.dao().Message().FindById(id);
    if (!msg) {
        return JsonError(resp, api_err::kMessageNotFound);
    }

    if (!ctx_.dao().Message().UpdateStatus(id, static_cast<int>(MsgStatus::kRecalled))) {
        return JsonError(resp, api_err::kRecallFailed);
    }

    // Push RecallNotify to all online conversation members so the recall
    // takes effect immediately (not only after the next sync)
    {
        proto::RecallNotify notify;
        notify.conversation_id = msg->conversation_id;
        notify.server_seq      = msg->seq;
        notify.operator_uid    = "";  // admin recall — no IM user uid

        Packet npkt;
        npkt.cmd  = static_cast<uint16_t>(Cmd::kRecallNotify);
        npkt.seq  = 0;
        npkt.uid  = 0;
        npkt.body = proto::Serialize(notify);

        std::string encoded = npkt.Encode();
        auto members = ctx_.dao().Conversation().GetMembersByConversation(msg->conversation_id);
        for (const auto& m : members) {
            auto conns = ctx_.conn_manager().GetConns(m.user_id);
            for (auto& c : conns) {
                c->SendEncoded(encoded);
                ctx_.incr_messages_out();
            }
        }
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "msg.recall", "message", id,
                  nlohmann::json({{"reason", reason}, {"conversation_id", msg->conversation_id}}).dump(),
                  GetClientIp(req));

    return JsonOk(resp);
}

// ============================================================
// 审计日志
// ============================================================

int AdminServer::HandleListAuditLogs(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.audit");
    if (rc != 0)
        return rc;

    auto pg          = ParsePagination(req);
    int64_t admin_id = 0;
    auto aid_str     = req->GetParam("admin_id");
    if (!aid_str.empty())
        admin_id = std::atoll(aid_str.c_str());

    std::string action     = req->GetParam("action");
    std::string start_time = req->GetParam("start_time");
    std::string end_time   = req->GetParam("end_time");

    auto result = ctx_.dao().AuditLog().List(admin_id, action, start_time, end_time, pg.page, pg.page_size);

    // 批量查询所有 admin uid（避免 N+1）
    std::vector<int64_t> admin_ids;
    admin_ids.reserve(result.items.size());
    for (const auto& log : result.items) {
        admin_ids.push_back(log.admin_id);
    }
    std::unordered_map<int64_t, std::string> uid_cache;
    for (auto aid : admin_ids) {
        if (uid_cache.find(aid) == uid_cache.end()) {
            auto a = ctx_.dao().AdminAccount().FindById(aid);
            uid_cache[aid] = a ? a->uid : "[deleted]";
        }
    }
    auto resolve_uid = [&](int64_t id) -> std::string {
        auto it = uid_cache.find(id);
        return it != uid_cache.end() ? it->second : "[deleted]";
    };

    nlohmann::json items = nlohmann::json::array();
    for (auto& log : result.items) {
        nlohmann::json item;
        item["id"]           = log.id;
        item["admin_id"]     = log.admin_id;
        item["operator_uid"] = resolve_uid(log.admin_id);
        item["action"]       = log.action;
        item["target_type"]  = log.target_type;
        item["target_id"]    = log.target_id;
        // detail 是 JSON 字符串，解析后嵌入
        item["detail"] = nlohmann::json::parse(log.detail, nullptr, false);
        if (item["detail"].is_discarded())
            item["detail"] = nlohmann::json::object();
        item["ip"]         = log.ip_address;
        item["created_at"] = log.created_at;
        items.push_back(item);
    }

    return JsonOk(resp, PaginatedResult(items, result.total, pg));
}

}  // namespace nova
