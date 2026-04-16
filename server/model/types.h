#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace nova {

// ---- ADL: struct → table name 映射 ----
// ormpp 通过 get_alias_struct_name(T*) 获取表名

struct User {
    int64_t id = 0;
    std::string uid;
    std::string password_hash;
    std::string nickname;
    std::string avatar;
    int status = 1;  // 1正常 2封禁 3已删除
    std::string created_at;
    std::string updated_at;
};
inline constexpr std::string_view get_alias_struct_name(User*) {
    return "users";
}

// 运维管理员（独立于 IM 用户）
struct Admin {
    int64_t id = 0;
    std::string uid;
    std::string password_hash;
    std::string nickname;
    int status = 1;  // 1正常 2封禁 3已删除
    std::string created_at;
    std::string updated_at;
};
inline constexpr std::string_view get_alias_struct_name(Admin*) {
    return "admins";
}

struct UserDevice {
    int64_t id      = 0;
    int64_t user_id = 0;
    std::string device_id;
    std::string device_type;
    std::string last_active_at;
};
inline constexpr std::string_view get_alias_struct_name(UserDevice*) {
    return "user_devices";
}

struct Message {
    int64_t id              = 0;
    int64_t conversation_id = 0;
    int64_t sender_id       = 0;
    int64_t seq             = 0;
    int msg_type            = 0;
    std::string content;
    int status = 0;  // 0正常 1已撤回 2已删除
    std::string client_msg_id;
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(Message*) {
    return "messages";
}

struct Conversation {
    int64_t id = 0;
    int type   = 0;  // 1私聊 2群聊
    std::string name;
    int64_t owner_id = 0;
    int64_t max_seq  = 0;
};
inline constexpr std::string_view get_alias_struct_name(Conversation*) {
    return "conversations";
}

struct ConversationMember {
    int64_t id              = 0;
    int64_t conversation_id = 0;
    int64_t user_id         = 0;
    int role                = 0;  // 0成员 1管理员 2群主
    int64_t last_read_seq   = 0;
    int64_t last_ack_seq    = 0;
    int mute                = 0;
};
inline constexpr std::string_view get_alias_struct_name(ConversationMember*) {
    return "conversation_members";
}

struct AuditLog {
    int64_t id       = 0;
    int64_t admin_id = 0;
    std::string action;       // e.g. "user.ban", "admin.login"
    std::string target_type;  // e.g. "user", "message"
    int64_t target_id = 0;
    std::string detail;       // JSON string
    std::string ip;
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(AuditLog*) {
    return "audit_logs";
}

struct AdminSession {
    int64_t id       = 0;
    int64_t admin_id = 0;
    std::string token_hash;  // SHA-256(JWT)
    std::string expires_at;
    int revoked = 0;         // 0有效 1已吊销
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(AdminSession*) {
    return "admin_sessions";
}

// RBAC 辅助结构（仅用于 ormpp 建表 + 查询）
struct Role {
    int64_t id = 0;
    std::string name;
    std::string code;
    std::string description;
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(Role*) {
    return "roles";
}

struct Permission {
    int64_t id = 0;
    std::string name;
    std::string code;
    int type = 0;
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(Permission*) {
    return "permissions";
}

struct RolePermission {
    int64_t id            = 0;
    int64_t role_id       = 0;
    int64_t permission_id = 0;
};
inline constexpr std::string_view get_alias_struct_name(RolePermission*) {
    return "role_permissions";
}

struct AdminRole {
    int64_t id       = 0;
    int64_t admin_id = 0;
    int64_t role_id  = 0;
};
inline constexpr std::string_view get_alias_struct_name(AdminRole*) {
    return "admin_roles";
}

}  // namespace nova
