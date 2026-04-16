#pragma once
// UserService 错误码

#include "common.h"

namespace nova::errc::user {

inline constexpr Error kUidRequired        {1,  "uid is required"};
inline constexpr Error kPasswordRequired   {1,  "password is required"};
inline constexpr Error kInvalidCredentials {2,  "invalid credentials"};
inline constexpr Error kUserBanned         {4,  "user is banned"};
inline constexpr Error kRateLimited        {5,  "too many attempts, try later"};

} // namespace nova::errc::user
