#pragma once
// SyncService 错误码

#include "common.h"

namespace nova::errc::sync {

inline constexpr Error kNotMember          {3,  "not a member of this conversation"};

} // namespace nova::errc::sync
