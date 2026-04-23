#include "admin_server.h"
#include "http_helper.h"
#include "jwt_utils.h"
#include "../core/password_utils.h"
#include "../core/server_context.h"
#include "../core/logger.h"
#include "../dao/dao_factory.h"
#include "../dao/user_dao.h"
#include "../dao/message_dao.h"
#include "../dao/audit_log_dao.h"
#include "../dao/admin_session_dao.h"
#include "../dao/admin_account_dao.h"
#include "../dao/rbac_dao.h"

#include <nova/protocol.h>
#include <nova/errors.h>
#include <hv/json.hpp>
#include "../core/events.h"

#include <mbedtls/sha256.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <functional>
#include <optional>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#else
#include <fstream>
#endif

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

// 获取当前进程内存使用量 (MB)
static double GetProcessMemoryMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
    return 0;
#else
    // Linux: 读 /proc/self/status 的 VmRSS
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            long long kb = 0;
            std::sscanf(line.c_str(), "VmRSS: %lld kB", &kb);
            return static_cast<double>(kb) / 1024.0;
        }
    }
    return 0;
#endif
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

    // 管理员管理
    service_.GET("/api/v1/admins", [this](auto* req, auto* resp) { return HandleListAdmins(req, resp); });
    service_.POST("/api/v1/admins", [this](auto* req, auto* resp) { return HandleCreateAdmin(req, resp); });
    service_.Delete("/api/v1/admins/:id", [this](auto* req, auto* resp) { return HandleDeleteAdmin(req, resp); });
    service_.POST("/api/v1/admins/:id/reset-password",
                  [this](auto* req, auto* resp) { return HandleResetAdminPassword(req, resp); });
    service_.POST("/api/v1/admins/:id/enable",
                  [this](auto* req, auto* resp) { return HandleEnableAdmin(req, resp); });
    service_.POST("/api/v1/admins/:id/disable",
                  [this](auto* req, auto* resp) { return HandleDisableAdmin(req, resp); });
    service_.PUT("/api/v1/admins/:id/roles",
                 [this](auto* req, auto* resp) { return HandleSetAdminRoles(req, resp); });

    // 角色管理
    service_.GET("/api/v1/roles", [this](auto* req, auto* resp) { return HandleListRoles(req, resp); });
    service_.POST("/api/v1/roles", [this](auto* req, auto* resp) { return HandleCreateRole(req, resp); });
    service_.PUT("/api/v1/roles/:id", [this](auto* req, auto* resp) { return HandleUpdateRole(req, resp); });
    service_.Delete("/api/v1/roles/:id", [this](auto* req, auto* resp) { return HandleDeleteRole(req, resp); });
    service_.GET("/api/v1/permissions", [this](auto* req, auto* resp) { return HandleListPermissions(req, resp); });
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

    // 防御纵深：显式拒绝 admin_id == 0 的 token（即使下游 FindById(0) 会返回空，
    // 提前拒绝避免审计日志打印无意义的 admin_id=0 记录，并防止未来代码误信任）
    if (claims->admin_id <= 0) {
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
    data["messages_today"] = ctx_.total_messages_in();  // 近似：使用入站计数
    data["bad_packets"]    = ctx_.bad_packets();
    data["uptime_seconds"] = ctx_.uptime_seconds();
    data["cpu_percent"]    = 0;  // 暂不采集 CPU（需要双次采样）
    data["memory_mb"]      = static_cast<int>(GetProcessMemoryMB());

    // ISO 8601 时间戳
    auto now   = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    char ts_buf[32];
    std::strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now_t));
    data["timestamp"] = ts_buf;

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
            {"id", u.id},
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

    // 昵称校验（trim + 非空 + 长度 + 控制字符）
    TrimInPlace(nickname);
    if (nickname.empty()) {
        return JsonError(resp, api_err::kNicknameRequired);
    }
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
    data["id"]         = user->id;
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

    KickAllConns(id);

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

    KickAllConns(id);

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

int AdminServer::KickAllConns(int64_t user_id) {
    int count = static_cast<int>(ctx_.conn_manager().GetConns(user_id).size());
    if (count > 0) {
        ctx_.bus().publish(event::topic::kAdminKickUser, event::AdminKickUser{user_id});
    }
    return count;
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

    int kicked = KickAllConns(id);
    if (kicked == 0) {
        return JsonError(resp, api_err::kUserNotOnline);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "user.kick", "user", id, "{}", GetClientIp(req));

    NOVA_NLOG_INFO(kLogTag, "kicked user {} ({} connections)", id, kicked);
    return JsonOk(resp, {{"kicked_devices", kicked}});
}

// ============================================================
// 消息管理
// ============================================================

int AdminServer::HandleListMessages(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "msg.view");
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
    int rc       = RequirePermission(req, resp, "msg.recall");
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

    // 通过消息总线通知 IM 侧广播撤回通知
    ctx_.bus().publish(event::topic::kAdminRecallMsg,
                       event::AdminRecallMsg{id, msg->conversation_id, msg->seq});

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

// ============================================================
// 管理员管理
// ============================================================

int AdminServer::HandleListAdmins(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto pg             = ParsePagination(req);
    std::string keyword = req->GetParam("keyword");

    auto result = ctx_.dao().AdminAccount().ListAdmins(keyword, pg.page, pg.page_size);

    nlohmann::json items = nlohmann::json::array();
    for (auto& a : result.items) {
        auto roles = ctx_.dao().Rbac().GetAdminRoles(a.id);
        nlohmann::json role_arr = nlohmann::json::array();
        for (auto& r : roles)
            role_arr.push_back(r);

        items.push_back({
            {"id", a.id},
            {"uid", a.uid},
            {"nickname", a.nickname},
            {"status", a.status},
            {"roles", role_arr},
            {"created_at", a.created_at},
            {"updated_at", a.updated_at},
        });
    }

    return JsonOk(resp, PaginatedResult(items, result.total, pg));
}

int AdminServer::HandleCreateAdmin(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("uid") || !body_opt->contains("password")) {
        return JsonError(resp, api_err::kUidPasswordRequired);
    }
    auto& body = *body_opt;

    std::string uid      = body.value("uid", "");
    std::string password = body.value("password", "");
    std::string nickname = body.value("nickname", "");

    TrimInPlace(uid);
    TrimInPlace(nickname);

    if (uid.empty() || password.size() < 6) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    // 检查是否已存在
    auto existing = ctx_.dao().AdminAccount().FindByUid(uid);
    if (existing) {
        return JsonError(resp, api_err::kUidAlreadyExists);
    }

    Admin admin;
    admin.uid           = uid;
    admin.password_hash = PasswordUtils::Hash(password);
    admin.nickname      = nickname.empty() ? uid : nickname;
    admin.status        = static_cast<int>(AccountStatus::Normal);

    // 安全：清除明文密码
    {
        volatile char* p = reinterpret_cast<volatile char*>(password.data());
        for (size_t i = 0; i < password.size(); ++i)
            p[i] = 0;
        password.clear();
    }

    if (!ctx_.dao().AdminAccount().Insert(admin)) {
        return JsonError(resp, api_err::kDatabaseError);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "admin.create", "admin", admin.id,
                  nlohmann::json({{"uid", uid}}).dump(), GetClientIp(req));

    return JsonOk(resp, nlohmann::json({{"id", admin.id}}));
}

int AdminServer::HandleDeleteAdmin(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto id_str = req->GetParam("id");
    int64_t id  = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    // 禁止删除 super admin (id=1) 或自己
    int64_t admin_id = GetCurrentAdminId(req);
    if (id == 1 || id == admin_id) {
        return JsonError(resp, api_err::kPermissionDenied);
    }

    if (!ctx_.dao().AdminAccount().SoftDelete(id)) {
        return JsonError(resp, api_err::kAdminNotFound);
    }

    WriteAuditLog(admin_id, "admin.delete", "admin", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleResetAdminPassword(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto id_str = req->GetParam("id");
    int64_t id  = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("new_password")) {
        return JsonError(resp, api_err::kNewPasswordRequired);
    }
    if (!(*body_opt)["new_password"].is_string()) {
        return JsonError(resp, api_err::kNewPasswordString);
    }

    std::string new_password = (*body_opt)["new_password"].get<std::string>();
    if (new_password.size() < 6) {
        return JsonError(resp, api_err::kPasswordTooShort);
    }
    if (new_password.size() > 128) {
        return JsonError(resp, api_err::kPasswordTooLong);
    }

    auto hash = PasswordUtils::Hash(new_password);
    {
        volatile char* p = reinterpret_cast<volatile char*>(new_password.data());
        for (size_t i = 0; i < new_password.size(); ++i)
            p[i] = 0;
        new_password.clear();
    }
    if (hash.empty()) {
        return JsonError(resp, api_err::kHashFailed);
    }

    if (!ctx_.dao().AdminAccount().UpdatePassword(id, hash)) {
        return JsonError(resp, api_err::kAdminNotFound);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "admin.reset_password", "admin", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleEnableAdmin(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto id_str = req->GetParam("id");
    int64_t id  = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    if (!ctx_.dao().AdminAccount().UpdateStatus(id, static_cast<int>(AccountStatus::Normal))) {
        return JsonError(resp, api_err::kAdminNotFound);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "admin.enable", "admin", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleDisableAdmin(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto id_str = req->GetParam("id");
    int64_t id  = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    // 禁止禁用自己
    int64_t admin_id = GetCurrentAdminId(req);
    if (id == admin_id) {
        return JsonError(resp, api_err::kPermissionDenied);
    }

    // 禁止禁用 super admin (id=1)
    if (id == 1) {
        return JsonError(resp, api_err::kPermissionDenied);
    }

    if (!ctx_.dao().AdminAccount().UpdateStatus(id, static_cast<int>(AccountStatus::Banned))) {
        return JsonError(resp, api_err::kAdminNotFound);
    }

    WriteAuditLog(admin_id, "admin.disable", "admin", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleSetAdminRoles(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto id_str = req->GetParam("id");
    int64_t id  = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("role_ids") || !(*body_opt)["role_ids"].is_array()) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    // 验证管理员存在
    auto target_admin = ctx_.dao().AdminAccount().FindById(id);
    if (!target_admin) {
        return JsonError(resp, api_err::kAdminNotFound);
    }

    // 获取所有角色以建立 name→id 映射
    auto all_roles = ctx_.dao().Rbac().ListRoles();
    std::unordered_map<int64_t, std::string> id_to_name;
    for (auto& r : all_roles) {
        id_to_name[r.id] = r.name;
    }

    // 先移除所有现有角色
    for (auto& r : all_roles) {
        ctx_.dao().Rbac().RemoveAdminRole(id, r.id);
    }

    // 添加新角色
    std::vector<std::string> new_role_names;
    for (auto& rid : (*body_opt)["role_ids"]) {
        if (rid.is_number_integer()) {
            int64_t role_id = rid.get<int64_t>();
            ctx_.dao().Rbac().AssignAdminRole(id, role_id);
            auto it = id_to_name.find(role_id);
            if (it != id_to_name.end())
                new_role_names.push_back(it->second);
        }
    }

    int64_t admin_id = GetCurrentAdminId(req);
    nlohmann::json detail_json;
    detail_json["role_ids"] = (*body_opt)["role_ids"];
    detail_json["role_names"] = new_role_names;
    WriteAuditLog(admin_id, "admin.set_roles", "admin", id, detail_json.dump(), GetClientIp(req));

    return JsonOk(resp);
}

// ============================================================
// 角色管理
// ============================================================

int AdminServer::HandleListRoles(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto roles = ctx_.dao().Rbac().ListRoles();

    nlohmann::json data = nlohmann::json::array();
    for (auto& r : roles) {
        nlohmann::json perm_arr = nlohmann::json::array();
        for (auto& p : r.permissions)
            perm_arr.push_back(p);

        data.push_back({
            {"id", r.id},
            {"name", r.name},
            {"description", r.description},
            {"permissions", perm_arr},
            {"created_at", r.created_at},
        });
    }

    return JsonOk(resp, data);
}

int AdminServer::HandleCreateRole(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto body_opt = ParseJsonBody(req);
    if (!body_opt || !body_opt->contains("name")) {
        return JsonError(resp, api_err::kInvalidParam);
    }
    auto& body = *body_opt;

    std::string name        = body.value("name", "");
    std::string description = body.value("description", "");
    TrimInPlace(name);

    if (name.empty()) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    if (!ctx_.dao().Rbac().CreateRole(name, description)) {
        return JsonError(resp, api_err::kDatabaseError);
    }

    // 查找刚创建的角色 ID
    auto roles = ctx_.dao().Rbac().ListRoles();
    int64_t new_role_id = 0;
    for (auto& r : roles) {
        if (r.name == name) {
            new_role_id = r.id;
            break;
        }
    }

    // 如果指定了权限，同时设置
    if (new_role_id > 0 && body.contains("permissions") && body["permissions"].is_array()) {
        std::vector<std::string> perms;
        for (auto& p : body["permissions"]) {
            if (p.is_string()) perms.push_back(p.get<std::string>());
        }
        ctx_.dao().Rbac().SetRolePermissions(new_role_id, perms);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "role.create", "role", new_role_id,
                  nlohmann::json({{"name", name}}).dump(), GetClientIp(req));

    return JsonOk(resp, nlohmann::json({{"id", new_role_id}}));
}

int AdminServer::HandleUpdateRole(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto id_str = req->GetParam("id");
    int64_t id  = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    auto body_opt = ParseJsonBody(req);
    if (!body_opt) {
        return JsonError(resp, api_err::kInvalidParam);
    }
    auto& body = *body_opt;

    if (body.contains("description") && body["description"].is_string()) {
        ctx_.dao().Rbac().UpdateRole(id, body["description"].get<std::string>());
    }

    if (body.contains("permissions") && body["permissions"].is_array()) {
        std::vector<std::string> perms;
        for (auto& p : body["permissions"]) {
            if (p.is_string()) perms.push_back(p.get<std::string>());
        }
        ctx_.dao().Rbac().SetRolePermissions(id, perms);
    }

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "role.update", "role", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleDeleteRole(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto id_str = req->GetParam("id");
    int64_t id  = std::atoll(id_str.c_str());
    if (id <= 0) {
        return JsonError(resp, api_err::kInvalidParam);
    }

    ctx_.dao().Rbac().DeleteRole(id);

    int64_t admin_id = GetCurrentAdminId(req);
    WriteAuditLog(admin_id, "role.delete", "role", id, "{}", GetClientIp(req));

    return JsonOk(resp);
}

int AdminServer::HandleListPermissions(HttpRequest* req, HttpResponse* resp) {
    auto session = ctx_.dao().Session();
    int rc       = RequirePermission(req, resp, "admin.manage");
    if (rc != 0)
        return rc;

    auto perms          = ctx_.dao().Rbac().ListPermissions();
    nlohmann::json data = nlohmann::json::array();
    for (auto& p : perms) {
        data.push_back(p);
    }

    return JsonOk(resp, data);
}

}  // namespace nova
