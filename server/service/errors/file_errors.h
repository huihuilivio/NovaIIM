#pragma once
// FileService 错误码

#include "common.h"

namespace nova::errc::file {

// clang-format off
inline constexpr Error kAvatarPathEmpty     {3001, "avatar path is empty"};
inline constexpr Error kAvatarPathTooLong   {3002, "avatar path exceeds 512 characters"};
inline constexpr Error kUpdateFailed        {3003, "failed to update avatar"};
inline constexpr Error kUserNotFound        {3004, "user not found"};
// clang-format on

}  // namespace nova::errc::file
