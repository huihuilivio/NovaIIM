#pragma once
// MsgService 错误码

#include "common.h"

namespace nova::errc::msg {

// clang-format off
inline constexpr Error kContentEmpty            {2001, "content is empty"};
inline constexpr Error kContentTooLarge         {2002, "content too large"};
inline constexpr Error kInvalidConversation     {2003, "invalid conversation_id"};
inline constexpr Error kConversationNotFound    {2004, "conversation not found"};
inline constexpr Error kNotMember               {2005, "not a member of this conversation"};
// clang-format on

}  // namespace nova::errc::msg
