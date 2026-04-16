#pragma once
// MsgService 错误码

#include "common.h"

namespace nova::errc::msg {

inline constexpr Error kContentEmpty       {1,  "content is empty"};
inline constexpr Error kContentTooLarge    {5,  "content too large"};
inline constexpr Error kInvalidConversation{6,  "invalid conversation_id"};
inline constexpr Error kConversationNotFound{6, "conversation not found"};
inline constexpr Error kNotMember          {7,  "not a member of this conversation"};

} // namespace nova::errc::msg
