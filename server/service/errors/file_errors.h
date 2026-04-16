#pragma once
// FileService 错误码

#include "common.h"

namespace nova::errc::file {

// clang-format off
inline constexpr Error kAvatarPathEmpty     {30, "avatar path is empty"};
inline constexpr Error kAvatarPathTooLong   {31, "avatar path exceeds 512 characters"};
inline constexpr Error kUpdateFailed        {32, "failed to update avatar"};
inline constexpr Error kUserNotFound        {33, "user not found"};
// clang-format on

}  // namespace nova::errc::file
