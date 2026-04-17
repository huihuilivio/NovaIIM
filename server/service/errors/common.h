#pragma once
// 公共错误码 —— 所有服务共用
// 约定：code=0 表示成功，<0 通用错误，>0 业务错误
// 业务错误码分段：user 1001-1099, msg 2001-2099, file 3001-3099, sync 4001-4099

#include <cstdint>

namespace nova::errc {

struct Error {
    int32_t code;
    const char* msg;
};

// ---- 通用 ----
// clang-format off
inline constexpr Error kOk                  {0,    "ok"};
inline constexpr Error kInvalidBody         {-1,   "invalid body"};
inline constexpr Error kNotAuthenticated    {-2,   "not authenticated"};
inline constexpr Error kDatabaseError       {-100, "database error"};
inline constexpr Error kServerBusy          {-503, "server busy"};
// clang-format on

}  // namespace nova::errc
