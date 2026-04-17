#pragma once
// UserService 错误码

#include "common.h"

namespace nova::errc::user {

// clang-format off
inline constexpr Error kEmailRequired       {1, "email is required"};
inline constexpr Error kInvalidCredentials  {2, "invalid credentials"};
inline constexpr Error kPasswordRequired    {3, "password is required"};
inline constexpr Error kUserBanned          {4, "user is banned"};
inline constexpr Error kRateLimited         {5, "too many attempts, try later"};

// ---- 注册 ----
inline constexpr Error kEmailInvalid        {6, "invalid email format"};
inline constexpr Error kEmailTooLong        {7, "email must be at most 255 characters"};
inline constexpr Error kEmailAlreadyExists  {8, "email already registered"};
inline constexpr Error kNicknameRequired    {10, "nickname is required"};
inline constexpr Error kNicknameTooLong     {11, "nickname must be at most 100 characters"};
inline constexpr Error kNicknameInvalid     {12, "nickname contains invalid characters"};
inline constexpr Error kPasswordTooShort    {13, "password must be at least 6 characters"};
inline constexpr Error kPasswordTooLong     {14, "password must be at most 128 characters"};
inline constexpr Error kRegisterFailed      {16, "registration failed"};

// ---- 搜索 / 资料 ----
inline constexpr Error kSearchKeywordEmpty  {17, "search keyword is required"};
inline constexpr Error kSearchKeywordTooLong{18, "search keyword too long"};
inline constexpr Error kUserNotFound        {19, "user not found"};
inline constexpr Error kNothingToUpdate     {20, "nothing to update"};
inline constexpr Error kUpdateProfileFailed {21, "failed to update profile"};
inline constexpr Error kAvatarPathTooLong   {22, "avatar path exceeds 512 characters"};
// clang-format on

}  // namespace nova::errc::user
