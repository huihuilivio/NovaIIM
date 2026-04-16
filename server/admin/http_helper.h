#pragma once

#include <hv/HttpMessage.h>
#include <hv/json.hpp>

#include <string>
#include <string_view>
#include <algorithm>

namespace nova {

// ============================================================
// 统一 JSON 响应
// ============================================================

// 错误码
enum class ApiCode : int {
    kOk           = 0,
    kParamError   = 1,
    kUnauthorized = 2,
    kForbidden    = 3,
    kNotFound     = 4,
    kInternal     = 5,
};

// 预定义 API 错误（消除 hardcode 字符串）
struct ApiError {
    ApiCode code;
    const char* msg;
    int http_status;
};

// clang-format off
namespace api_err {
// ---- 通用 ----
inline constexpr ApiError kBodyTooLarge         {ApiCode::kParamError,      "request body too large",                   413};
inline constexpr ApiError kInvalidParam         {ApiCode::kParamError,      "invalid parameter",                        400};
inline constexpr ApiError kInvalidUserId        {ApiCode::kParamError,      "invalid user id",                          400};
inline constexpr ApiError kInvalidMessageId     {ApiCode::kParamError,      "invalid message id",                       400};

// ---- 认证 ----
inline constexpr ApiError kMissingToken         {ApiCode::kUnauthorized,    "missing or invalid token",                 401};
inline constexpr ApiError kTokenExpired         {ApiCode::kUnauthorized,    "invalid or expired token",                 401};
inline constexpr ApiError kTokenRevoked         {ApiCode::kUnauthorized,    "token has been revoked",                   401};
inline constexpr ApiError kInvalidCredentials   {ApiCode::kUnauthorized,    "invalid credentials",                      401};
inline constexpr ApiError kNotAuthenticated     {ApiCode::kUnauthorized,    "not authenticated",                        401};

// ---- 权限 / 频率 ----
inline constexpr ApiError kRateLimited          {ApiCode::kForbidden,       "too many login attempts, try again later", 429};
inline constexpr ApiError kAccountDisabled      {ApiCode::kForbidden,       "account is disabled",                      403};
inline constexpr ApiError kNoAdminAccess        {ApiCode::kForbidden,       "no admin access",                          403};
inline constexpr ApiError kPermissionDenied     {ApiCode::kForbidden,       "permission denied",                        403};

// ---- 参数校验 ----
inline constexpr ApiError kUidPasswordRequired  {ApiCode::kParamError,      "uid and password required",                400};
inline constexpr ApiError kUidPasswordStrings   {ApiCode::kParamError,      "uid and password must be strings",         400};
inline constexpr ApiError kUidPasswordEmpty     {ApiCode::kParamError,      "uid and password cannot be empty",         400};
inline constexpr ApiError kUidAlreadyExists     {ApiCode::kParamError,      "uid already exists",                       409};
inline constexpr ApiError kNewPasswordRequired  {ApiCode::kParamError,      "new_password required",                    400};
inline constexpr ApiError kNewPasswordString    {ApiCode::kParamError,      "new_password must be a string",            400};
inline constexpr ApiError kNewPasswordEmpty     {ApiCode::kParamError,      "new_password cannot be empty",             400};

// ---- 资源不存在 ----
inline constexpr ApiError kUserNotFound         {ApiCode::kNotFound,        "user not found",                           200};
inline constexpr ApiError kAdminNotFound        {ApiCode::kNotFound,        "admin not found",                          200};
inline constexpr ApiError kMessageNotFound      {ApiCode::kNotFound,        "message not found",                        200};
inline constexpr ApiError kUserNotOnline        {ApiCode::kNotFound,        "user not online",                          200};

// ---- 内部错误 ----
inline constexpr ApiError kHashFailed           {ApiCode::kInternal,        "failed to hash password",                  500};
inline constexpr ApiError kSignTokenFailed      {ApiCode::kInternal,        "failed to sign token",                     500};
inline constexpr ApiError kCreateUserFailed     {ApiCode::kInternal,        "failed to create user",                    500};
inline constexpr ApiError kRecallFailed         {ApiCode::kInternal,        "failed to recall message",                 500};
}  // namespace api_err
// clang-format on

inline int JsonOk(HttpResponse* resp, const nlohmann::json& data = nullptr) {
    resp->content_type = APPLICATION_JSON;
    nlohmann::json j;
    j["code"]  = 0;
    j["msg"]   = "ok";
    j["data"]  = data.is_null() ? nlohmann::json::object() : data;
    resp->body = j.dump();
    return 200;
}

inline int JsonError(HttpResponse* resp, ApiCode code, std::string_view msg, int http_status = 200) {
    resp->content_type = APPLICATION_JSON;
    nlohmann::json j;
    j["code"]  = static_cast<int>(code);
    j["msg"]   = msg;
    j["data"]  = nlohmann::json::object();
    resp->body = j.dump();
    return http_status;
}

// 便捷重载：直接传 ApiError 常量
inline int JsonError(HttpResponse* resp, const ApiError& err) {
    return JsonError(resp, err.code, err.msg, err.http_status);
}

// ============================================================
// 分页
// ============================================================

struct Pagination {
    int page      = 1;
    int page_size = 20;

    int64_t Offset() const { return static_cast<int64_t>(page - 1) * page_size; }
};

inline Pagination ParsePagination(HttpRequest* req) {
    Pagination p;
    auto page_str = req->GetParam("page");
    auto size_str = req->GetParam("page_size");
    if (!page_str.empty())
        p.page = std::clamp(std::atoi(page_str.c_str()), 1, 10000);
    if (!size_str.empty())
        p.page_size = std::clamp(std::atoi(size_str.c_str()), 1, 100);
    return p;
}

inline nlohmann::json PaginatedResult(const nlohmann::json& items, int64_t total, const Pagination& p) {
    return {
        {"items", items},
        {"total", total},
        {"page", p.page},
        {"page_size", p.page_size},
    };
}

// ============================================================
// 权限检查 (Phase 2 使用，依赖 req context 中的 permissions)
// ============================================================

// 从请求中获取当前管理员 ID（由 AuthMiddleware 注入）
inline int64_t GetCurrentAdminId(HttpRequest* req) {
    auto val = req->GetHeader("X-Nova-Admin-Id");
    return val.empty() ? 0 : std::atoll(val.c_str());
}

// 从请求中获取权限列表（由 AuthMiddleware 注入，逗号分隔）
inline bool HasPermission(HttpRequest* req, std::string_view perm) {
    auto perms = req->GetHeader("X-Nova-Permissions");
    if (perms.empty())
        return false;
    // 按逗号分割，精确匹配
    size_t start = 0;
    while (start < perms.size()) {
        auto end = perms.find(',', start);
        if (end == std::string::npos)
            end = perms.size();
        if (perms.substr(start, end - start) == perm)
            return true;
        start = end + 1;
    }
    return false;
}

inline int RequirePermission(HttpRequest* req, HttpResponse* resp, std::string_view perm) {
    if (!HasPermission(req, perm)) {
        return JsonError(resp, api_err::kPermissionDenied);
    }
    return 0;  // 0 表示通过
}

}  // namespace nova
