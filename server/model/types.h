#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace nova {

// ---- 状态 / 角色 枚举 ----

// User / Admin 通用状态
enum class AccountStatus : int { Normal = 1, Banned = 2, Deleted = 3 };

// Message 状态
enum class MsgStatus : int { Normal = 0, Recalled = 1, Deleted = 2 };

// Conversation 类型
enum class ConvType : int { Private = 1, Group = 2 };

// ConversationMember 角色
enum class MemberRole : int { Member = 0, Admin = 1, Owner = 2 };

// UserFile 状态
enum class FileStatus : int { Active = 1, Deleted = 2 };

// AdminSession 吊销状态
enum class SessionRevoked : int { Valid = 0, Revoked = 1 };

// ---- ADL: struct → table name 映射 ----
// ormpp 通过 get_alias_struct_name(T*) 获取表名

struct User {
    int64_t id = 0;
    std::string uid;
    std::string email;
    std::string password_hash;
    std::string nickname;
    std::string avatar;
    int status = static_cast<int>(AccountStatus::Normal);
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
    int status = static_cast<int>(AccountStatus::Normal);
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
    std::string created_at;
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
    std::string encrypted_content;
    int status = static_cast<int>(MsgStatus::Normal);
    std::string client_msg_id;
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(Message*) {
    return "messages";
}

struct Conversation {
    int64_t id = 0;
    int type   = 0;
    std::string name;
    std::string avatar;
    int64_t owner_id = 0;
    int64_t max_seq  = 0;
    std::string created_at;
    std::string updated_at;
};
inline constexpr std::string_view get_alias_struct_name(Conversation*) {
    return "conversations";
}

struct ConversationMember {
    int64_t id              = 0;
    int64_t conversation_id = 0;
    int64_t user_id         = 0;
    int role                = static_cast<int>(MemberRole::Member);
    int64_t last_read_seq   = 0;
    int64_t last_ack_seq    = 0;
    int mute                = 0;
    std::string joined_at;
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
    std::string ip_address;
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
    int revoked = static_cast<int>(SessionRevoked::Valid);
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
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(AdminRole*) {
    return "admin_roles";
}

// 文件记录（通用文件元数据，支持头像、图片、语音等）
struct UserFile {
    int64_t id      = 0;
    int64_t user_id = 0;
    std::string file_type;  // "avatar", "image", "voice", "video", "file"
    std::string file_path;  // 存储路径（相对或 URL）
    std::string file_name;  // 原始文件名
    int64_t file_size = 0;  // 字节数
    std::string mime_type;  // e.g. "image/png"
    std::string hash;       // 文件哈希（去重 / 校验）
    int status = static_cast<int>(FileStatus::Active);
    std::string created_at;
    std::string updated_at;
};
inline constexpr std::string_view get_alias_struct_name(UserFile*) {
    return "user_files";
}

}  // namespace nova
