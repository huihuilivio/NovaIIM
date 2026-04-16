#pragma once
// SyncService 错误码

#include "common.h"

namespace nova::errc::sync {

// clang-format off
inline constexpr Error kNotMember {3, "not a member of this conversation"};
// clang-format on

}  // namespace nova::errc::sync
