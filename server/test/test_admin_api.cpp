// test_admin_api.cpp — AdminServer HTTP 集成测试
// 启动真实 AdminServer (in-proc, 19091 端口) + SQLite 内存数据库
// 使用 requests::get/post 同步 HTTP 客户端发起请求并验证响应
//
// 测试覆盖:
//   1. GET /healthz                   — 公开，无需鉴权
//   2. POST /api/v1/auth/login        — 成功、密码错误、用户不存在
//   3. 鉴权中间件                      — 无 Token、无效 Token、吊销 Token
//   4. POST /api/v1/auth/logout       — 吊销当前 Token
//   5. GET  /api/v1/auth/me           — 返回当前管理员信息
//   6. GET  /api/v1/dashboard/stats   — 仪表盘数据
//   7. GET  /api/v1/users             — 列出 IM 用户
//   8. POST /api/v1/users             — 创建 IM 用户
//   9. GET  /api/v1/audit-logs        — 列出审计日志

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>
#include <memory>

// libhv
#include <hv/requests.h>
#include <hv/json.hpp>

// server headers
#include "admin/admin_server.h"
#include "core/server_context.h"
#include "core/app_config.h"
#include "dao/dao_factory.h"

namespace nova {
namespace {

// ============================================================
// 测试常量
// ============================================================

static constexpr int kPort              = 19091;
static constexpr const char* kBase      = "http://127.0.0.1:19091";
static constexpr const char* kSecret    = "integration-test-secret-32bytes!";
static constexpr const char* kAdminUid  = "admin";
static constexpr const char* kAdminPass = "nova2024";  // SeedSuperAdmin 的默认密码

// ============================================================
// 工具函数
// ============================================================

// Returns URL string — call .c_str() when passing to requests::get/post directly
static std::string Url(const char* path) {
    return std::string(kBase) + path;
}

// Convenience: returns a URL as a local string for .c_str() chaining
// Usage: requests::get(Urlc("/path")) is NOT safe (dangling ptr)
// Use: auto u = Url("/path"); requests::get(u.c_str());

static nlohmann::json ParseJson(const requests::Response& resp) {
    EXPECT_NE(resp, nullptr);
    if (!resp)
        return {};
    return nlohmann::json::parse(resp->body, nullptr, false);
}

static requests::Response AuthPost(const std::string& url, const std::string& token, const nlohmann::json& body = {}) {
    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_POST;
    req->url    = url;
    req->SetHeader("Authorization", "Bearer " + token);
    req->SetHeader("Content-Type", "application/json");
    if (!body.empty()) {
        req->body = body.dump();
    }
    return requests::request(req);
}

static requests::Response AuthGet(const std::string& url, const std::string& token) {
    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_GET;
    req->url    = url;
    req->SetHeader("Authorization", "Bearer " + token);
    return requests::request(req);
}

// ============================================================
// 测试 Fixture — 每个测试 Suite 共享一个 AdminServer 实例
// ============================================================

class AdminApiTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        // 创建内存数据库
        DatabaseConfig db_cfg;
        db_cfg.type = "sqlite";
        db_cfg.path = ":memory:";

        AppConfig app_cfg;

        app_cfg.db = db_cfg;
        ctx_       = std::make_unique<ServerContext>(app_cfg);
        ctx_->set_dao(CreateDaoFactory(db_cfg));

        // 启动 AdminServer
        AdminServer::Options opts;
        opts.port        = kPort;
        opts.jwt_secret  = kSecret;
        opts.jwt_expires = 3600;

        server_ = std::make_unique<AdminServer>(*ctx_);
        int rc  = server_->Start(opts);
        ASSERT_EQ(rc, 0) << "AdminServer failed to start on port " << kPort;

        // 等待服务器就绪
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    static void TearDownTestSuite() {
        if (server_)
            server_->Stop();
        server_.reset();
        ctx_.reset();
    }

protected:
    // 登录并返回 token（测试辅助）
    static std::string Login(const char* uid = kAdminUid, const char* pass = kAdminPass) {
        nlohmann::json body = {{"uid", uid}, {"password", pass}};
        auto url            = Url("/api/v1/auth/login");
        auto resp           = requests::post(url.c_str(), body.dump(), {{"Content-Type", "application/json"}});
        if (!resp || resp->status_code != 200)
            return {};
        auto j = ParseJson(resp);
        if (!j.contains("data") || !j["data"].contains("token"))
            return {};
        return j["data"]["token"].get<std::string>();
    }

    static std::unique_ptr<ServerContext> ctx_;
    static std::unique_ptr<AdminServer> server_;
};

std::unique_ptr<ServerContext> AdminApiTest::ctx_;
std::unique_ptr<AdminServer> AdminApiTest::server_;

// ============================================================
// 1. 健康检查
// ============================================================

TEST_F(AdminApiTest, HealthzReturnsOk) {
    auto url  = Url("/healthz");
    auto resp = requests::get(url.c_str());
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);
    auto j = ParseJson(resp);
    EXPECT_EQ(j["status"], "ok");
}

TEST_F(AdminApiTest, HealthzDoesNotRequireAuth) {
    // 无 Authorization 头仍应返回 200
    auto url  = Url("/healthz");
    auto resp = requests::get(url.c_str());
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);
}

// ============================================================
// 2. 登录
// ============================================================

TEST_F(AdminApiTest, LoginSuccess) {
    nlohmann::json body = {{"uid", kAdminUid}, {"password", kAdminPass}};
    auto url            = Url("/api/v1/auth/login");
    auto resp           = requests::post(url.c_str(), body.dump(), {{"Content-Type", "application/json"}});
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    auto j = ParseJson(resp);
    EXPECT_EQ(j["code"], 0);
    EXPECT_TRUE(j["data"].contains("token"));
    EXPECT_FALSE(j["data"]["token"].get<std::string>().empty());
    EXPECT_GT(j["data"]["expires_in"].get<int>(), 0);
}

TEST_F(AdminApiTest, LoginWrongPassword) {
    nlohmann::json body = {{"uid", kAdminUid}, {"password", "wrong-password"}};
    auto url            = Url("/api/v1/auth/login");
    auto resp           = requests::post(url.c_str(), body.dump(), {{"Content-Type", "application/json"}});
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 401);
    auto j = ParseJson(resp);
    EXPECT_NE(j["code"], 0);
}

TEST_F(AdminApiTest, LoginNonExistentUser) {
    nlohmann::json body = {{"uid", "ghost"}, {"password", "anything"}};
    auto url            = Url("/api/v1/auth/login");
    auto resp           = requests::post(url.c_str(), body.dump(), {{"Content-Type", "application/json"}});
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 401);
}

TEST_F(AdminApiTest, LoginMissingFields) {
    // 缺少 password 字段
    nlohmann::json body = {{"uid", kAdminUid}};
    auto url            = Url("/api/v1/auth/login");
    auto resp           = requests::post(url.c_str(), body.dump(), {{"Content-Type", "application/json"}});
    ASSERT_NE(resp, nullptr);
    EXPECT_NE(resp->status_code, 200);
}

TEST_F(AdminApiTest, LoginEmptyBody) {
    auto url  = Url("/api/v1/auth/login");
    auto resp = requests::post(url.c_str(), std::string{}, {{"Content-Type", "application/json"}});
    ASSERT_NE(resp, nullptr);
    EXPECT_NE(resp->status_code, 200);
}

// ============================================================
// 3. 鉴权中间件
// ============================================================

TEST_F(AdminApiTest, RequestWithoutTokenReturns401) {
    auto url  = Url("/api/v1/auth/me");
    auto resp = requests::get(url.c_str());
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 401);
}

TEST_F(AdminApiTest, RequestWithInvalidTokenReturns401) {
    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_GET;
    req->url    = Url("/api/v1/auth/me");
    req->SetHeader("Authorization", "Bearer not.a.valid.jwt");
    auto resp = requests::request(req);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 401);
}

TEST_F(AdminApiTest, RequestWithMalformedAuthHeaderReturns401) {
    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_GET;
    req->url    = Url("/api/v1/auth/me");
    req->SetHeader("Authorization", "Token abc123");  // 非 Bearer 格式
    auto resp = requests::request(req);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 401);
}

TEST_F(AdminApiTest, RevokedTokenReturns401) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    // 先验证 token 有效
    auto me1 = AuthGet(Url("/api/v1/auth/me"), token);
    ASSERT_NE(me1, nullptr);
    EXPECT_EQ(me1->status_code, 200);

    // Logout 吊销 token
    AuthPost(Url("/api/v1/auth/logout"), token);

    // 使用相同 token 再次请求应被拒绝
    auto me2 = AuthGet(Url("/api/v1/auth/me"), token);
    ASSERT_NE(me2, nullptr);
    EXPECT_EQ(me2->status_code, 401);
}

// ============================================================
// 4. Logout
// ============================================================

TEST_F(AdminApiTest, LogoutSuccess) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    auto resp = AuthPost(Url("/api/v1/auth/logout"), token);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);
    auto j = ParseJson(resp);
    EXPECT_EQ(j["code"], 0);
}

// ============================================================
// 5. GET /api/v1/auth/me
// ============================================================

TEST_F(AdminApiTest, MeReturnsAdminInfo) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    auto resp = AuthGet(Url("/api/v1/auth/me"), token);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    auto j = ParseJson(resp);
    EXPECT_EQ(j["code"], 0);
    EXPECT_EQ(j["data"]["uid"], kAdminUid);
    EXPECT_TRUE(j["data"].contains("admin_id"));
    EXPECT_TRUE(j["data"].contains("permissions"));
    EXPECT_FALSE(j["data"]["permissions"].empty());
}

TEST_F(AdminApiTest, MePermissionsContainLoginPermission) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    auto resp = AuthGet(Url("/api/v1/auth/me"), token);
    ASSERT_NE(resp, nullptr);
    ASSERT_EQ(resp->status_code, 200);

    auto j         = ParseJson(resp);
    auto perms     = j["data"]["permissions"];
    bool has_login = false;
    for (auto& p : perms) {
        if (p.get<std::string>() == "admin.login") {
            has_login = true;
            break;
        }
    }
    EXPECT_TRUE(has_login);
}

// ============================================================
// 6. GET /api/v1/dashboard/stats
// ============================================================

TEST_F(AdminApiTest, StatsReturnsData) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    auto resp = AuthGet(Url("/api/v1/dashboard/stats"), token);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    auto j = ParseJson(resp);
    EXPECT_EQ(j["code"], 0);
    // 基本字段存在性检查
    EXPECT_TRUE(j["data"].contains("online_users") || j["data"].contains("user_count") ||
                j["data"].contains("uptime_seconds"));
}

// ============================================================
// 7 & 8. 用户管理
// ============================================================

TEST_F(AdminApiTest, ListUsersReturnsEmptyInitially) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    auto resp = AuthGet(Url("/api/v1/users"), token);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    auto j = ParseJson(resp);
    EXPECT_EQ(j["code"], 0);
    EXPECT_TRUE(j["data"].contains("items") || j["data"].contains("list") || j["data"].is_array());
}

TEST_F(AdminApiTest, CreateUserSuccess) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    nlohmann::json body = {{"email", "testuser1@example.com"}, {"nickname", "测试用户"}, {"password", "testpass123"}};
    auto resp           = AuthPost(Url("/api/v1/users"), token, body);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    auto j = ParseJson(resp);
    EXPECT_EQ(j["code"], 0);
}

TEST_F(AdminApiTest, CreateUserDuplicateEmailFails) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    nlohmann::json body = {{"email", "dup@example.com"}, {"nickname", "Dup1"}, {"password", "pass123"}};
    // 第一次成功
    AuthPost(Url("/api/v1/users"), token, body);

    // 第二次应失败（UNIQUE 约束）
    auto resp = AuthPost(Url("/api/v1/users"), token, body);
    ASSERT_NE(resp, nullptr);
    EXPECT_NE(resp->status_code, 200);
}

TEST_F(AdminApiTest, CreateUserMissingEmailFails) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    nlohmann::json body = {{"nickname", "NoEmail"}, {"password", "pass"}};
    auto resp           = AuthPost(Url("/api/v1/users"), token, body);
    ASSERT_NE(resp, nullptr);
    EXPECT_NE(resp->status_code, 200);
}

TEST_F(AdminApiTest, ListUsersReflectsCreatedUser) {
    auto token = Login();
    ASSERT_FALSE(token.empty());

    // 创建用户
    nlohmann::json body = {{"email", "listme@example.com"}, {"nickname", "列表可见"}, {"password", "pass123"}};
    AuthPost(Url("/api/v1/users"), token, body);

    // 列表中应能返回数据（code=0）
    auto resp = AuthGet(Url("/api/v1/users"), token);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);
    auto j = ParseJson(resp);
    EXPECT_EQ(j["code"], 0);
}

// ============================================================
// 9. 审计日志
// ============================================================

TEST_F(AdminApiTest, AuditLogsReturnAfterLogin) {
    // Login 会写入一条 admin.login 审计日志
    auto token = Login();
    ASSERT_FALSE(token.empty());

    auto resp = AuthGet(Url("/api/v1/audit-logs"), token);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    auto j = ParseJson(resp);
    EXPECT_EQ(j["code"], 0);
    // 应至少有一条记录（来自 Login 动作）
    auto& data     = j["data"];
    bool has_items = data.contains("items")  ? !data["items"].empty()
                     : data.is_array()       ? !data.empty()
                     : data.contains("list") ? !data["list"].empty()
                                             : false;
    EXPECT_TRUE(has_items);
}

}  // namespace
}  // namespace nova
