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
inline constexpr Error kUidTooShort         {10, "uid must be at least 3 characters"};
inline constexpr Error kUidTooLong          {11, "uid must be at most 32 characters"};
inline constexpr Error kUidInvalidChars     {12, "uid may only contain a-z, 0-9, _ and -"};
inline constexpr Error kPasswordTooShort    {13, "password must be at least 6 characters"};
inline constexpr Error kPasswordTooLong     {14, "password must be at most 128 characters"};
inline constexpr Error kUidAlreadyExists    {15, "uid already exists"};
inline constexpr Error kRegisterFailed      {16, "registration failed"};
// clang-format on

}  // namespace nova::errc::user
