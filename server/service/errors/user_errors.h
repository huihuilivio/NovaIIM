#pragma once
// UserService 错误码

#include "common.h"

namespace nova::errc::user {

// clang-format off
inline constexpr Error kEmailRequired       {1001, "email is required"};
inline constexpr Error kInvalidCredentials  {1002, "invalid credentials"};
inline constexpr Error kPasswordRequired    {1003, "password is required"};
inline constexpr Error kUserBanned          {1004, "user is banned"};
inline constexpr Error kRateLimited         {1005, "too many attempts, try later"};

// ---- 注册 ----
inline constexpr Error kEmailInvalid        {1006, "invalid email format"};
inline constexpr Error kEmailTooLong        {1007, "email must be at most 255 characters"};
inline constexpr Error kEmailAlreadyExists  {1008, "email already registered"};
inline constexpr Error kNicknameRequired    {1010, "nickname is required"};
inline constexpr Error kNicknameTooLong     {1011, "nickname must be at most 100 characters"};
inline constexpr Error kNicknameInvalid     {1012, "nickname contains invalid characters"};
inline constexpr Error kPasswordTooShort    {1013, "password must be at least 6 characters"};
inline constexpr Error kPasswordTooLong     {1014, "password must be at most 128 characters"};
inline constexpr Error kRegisterFailed      {1016, "registration failed"};

// ---- 搜索 / 资料 ----
inline constexpr Error kSearchKeywordEmpty  {1017, "search keyword is required"};
inline constexpr Error kSearchKeywordTooLong{1018, "search keyword too long"};
inline constexpr Error kUserNotFound        {1019, "user not found"};
inline constexpr Error kNothingToUpdate     {1020, "nothing to update"};
inline constexpr Error kUpdateProfileFailed {1021, "failed to update profile"};
inline constexpr Error kAvatarPathTooLong   {1022, "avatar path exceeds 512 characters"};
// clang-format on

}  // namespace nova::errc::user
