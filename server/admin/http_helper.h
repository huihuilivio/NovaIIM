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

inline int JsonOk(HttpResponse* resp, const nlohmann::json& data = nullptr) {
    resp->content_type = APPLICATION_JSON;
    nlohmann::json j;
    j["code"] = 0;
    j["msg"]  = "ok";
    j["data"] = data.is_null() ? nlohmann::json::object() : data;
    resp->body = j.dump();
    return 200;
}

inline int JsonError(HttpResponse* resp, ApiCode code, std::string_view msg, int http_status = 200) {
    resp->content_type = APPLICATION_JSON;
    nlohmann::json j;
    j["code"] = static_cast<int>(code);
    j["msg"]  = msg;
    j["data"] = nlohmann::json::object();
    resp->body = j.dump();
    return http_status;
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
    if (!page_str.empty()) p.page = std::clamp(std::atoi(page_str.c_str()), 1, 10000);
    if (!size_str.empty()) p.page_size = std::clamp(std::atoi(size_str.c_str()), 1, 100);
    return p;
}

inline nlohmann::json PaginatedResult(const nlohmann::json& items, int64_t total, const Pagination& p) {
    return {
        {"items",     items},
        {"total",     total},
        {"page",      p.page},
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
    if (perms.empty()) return false;
    // 按逗号分割，精确匹配
    size_t start = 0;
    while (start < perms.size()) {
        auto end = perms.find(',', start);
        if (end == std::string::npos) end = perms.size();
        if (perms.substr(start, end - start) == perm) return true;
        start = end + 1;
    }
    return false;
}

inline int RequirePermission(HttpRequest* req, HttpResponse* resp, std::string_view perm) {
    if (!HasPermission(req, perm)) {
        return JsonError(resp, ApiCode::kForbidden, "permission denied", 403);
    }
    return 0;  // 0 表示通过
}

} // namespace nova
