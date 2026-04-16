#pragma once
// UserService 错误码

#include "common.h"

namespace nova::errc::user {

// clang-format off
inline constexpr Error kUidRequired         {1, "uid is required"};
inline constexpr Error kPasswordRequired    {1, "password is required"};
inline constexpr Error kInvalidCredentials  {2, "invalid credentials"};
inline constexpr Error kUserBanned          {4, "user is banned"};
inline constexpr Error kRateLimited         {5, "too many attempts, try later"};

// ---- 注册 ----
inline constexpr Error kNicknameRequired    {10, "nickname is required"};
inline constexpr Error kPasswordTooShort    {13, "password must be at least 6 characters"};
inline constexpr Error kPasswordTooLong     {14, "password must be at most 128 characters"};
inline constexpr Error kRegisterFailed      {16, "registration failed"};
// clang-format on

}  // namespace nova::errc::user
