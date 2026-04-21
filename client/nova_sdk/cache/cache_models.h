#pragma once
// cache_models.h — SDK 本地缓存数据模型
//
// 客户端 SQLite 缓存表结构，使用 ormpp + iguana 自动反射。
// 所有模型遵循 ormpp 惯例：
//   - int64_t id 作为自增主键
//   - get_alias_struct_name(T*) ADL 映射表名
//   - 简单聚合体，C++20 编译期反射

#include <cstdint>
#include <string>
#include <string_view>

namespace nova::client {

// ---- 用户信息缓存 ----
struct CachedUser {
    int64_t id = 0;
    std::string uid;
    std::string nickname;
    std::string avatar;
    std::string email;
    std::string updated_at;
};
inline constexpr std::string_view get_alias_struct_name(CachedUser*) {
    return "cached_users";
}

// ---- 好友缓存 ----
struct CachedFriend {
    int64_t id = 0;
    std::string uid;
    std::string nickname;
    std::string avatar;
    int64_t conversation_id = 0;
    std::string updated_at;
};
inline constexpr std::string_view get_alias_struct_name(CachedFriend*) {
    return "cached_friends";
}

// ---- 消息缓存 ----
struct CachedMessage {
    int64_t id = 0;
    int64_t conversation_id = 0;
    std::string sender_uid;
    std::string content;
    int msg_type = 0;
    int64_t server_seq = 0;
    int64_t server_time = 0;
    int status = 0;              // 0=normal, 1=recalled, 2=deleted
    std::string client_msg_id;
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(CachedMessage*) {
    return "cached_messages";
}

// ---- 会话缓存 ----
struct CachedConversation {
    int64_t id = 0;
    int64_t conversation_id = 0;
    int type = 0;                // 1=私聊, 2=群聊
    std::string name;
    std::string avatar;
    int64_t unread_count = 0;
    int mute = 0;
    int pinned = 0;
    std::string last_msg_sender_uid;
    std::string last_msg_sender_nickname;
    std::string last_msg_content;
    int last_msg_type = 0;
    int64_t last_msg_time = 0;
    std::string updated_at;
};
inline constexpr std::string_view get_alias_struct_name(CachedConversation*) {
    return "cached_conversations";
}

// ---- 群组信息缓存 ----
struct CachedGroup {
    int64_t id = 0;
    int64_t conversation_id = 0;
    std::string name;
    std::string avatar;
    int64_t owner_id = 0;
    std::string notice;
    int member_count = 0;
    std::string created_at;
};
inline constexpr std::string_view get_alias_struct_name(CachedGroup*) {
    return "cached_groups";
}

// ---- 群成员缓存 ----
struct CachedGroupMember {
    int64_t id = 0;
    int64_t conversation_id = 0;
    int64_t user_id = 0;
    std::string uid;
    std::string nickname;
    std::string avatar;
    int role = 0;                // 0=成员, 1=管理员, 2=群主
    std::string joined_at;
};
inline constexpr std::string_view get_alias_struct_name(CachedGroupMember*) {
    return "cached_group_members";
}

// ---- 同步状态追踪 ----
struct SyncState {
    int64_t id = 0;
    int64_t conversation_id = 0;
    int64_t last_sync_seq = 0;
};
inline constexpr std::string_view get_alias_struct_name(SyncState*) {
    return "sync_state";
}

}  // namespace nova::client
