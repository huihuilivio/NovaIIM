#pragma once

#include <cstdint>
#include <string>

namespace nova::event {

// Admin → IM 事件主题常量
namespace topic {
    inline constexpr const char* kAdminKickUser    = "admin/kick_user";
    inline constexpr const char* kAdminRecallMsg   = "admin/recall_msg";
}  // namespace topic

// Admin 踢用户下线（ban / delete / kick 共用）
struct AdminKickUser {
    int64_t user_id = 0;  // 内部 id（非 uid 字符串）
};

// Admin 撤回消息
struct AdminRecallMsg {
    int64_t message_id       = 0;
    int64_t conversation_id  = 0;
    int64_t server_seq       = 0;
};

}  // namespace nova::event
