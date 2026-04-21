#include "user_cache_dao.h"

#include <infra/logger.h>

namespace nova::client {

// ---- 用户缓存 ----

void UserCacheDao::UpsertUser(const CachedUser& user) {
    auto& conn = db_.DB();
    auto existing = conn.query_s<CachedUser>("uid=?", user.uid);
    if (!existing.empty()) {
        auto updated = existing[0];
        updated.nickname = user.nickname;
        updated.avatar = user.avatar;
        updated.email = user.email;
        updated.updated_at = user.updated_at;
        conn.update(updated);
    } else {
        conn.insert(user);
    }
}

std::optional<CachedUser> UserCacheDao::GetUser(const std::string& uid) {
    auto res = db_.DB().query_s<CachedUser>("uid=?", uid);
    if (res.empty()) return std::nullopt;
    return res[0];
}

std::vector<CachedUser> UserCacheDao::GetAllUsers() {
    return db_.DB().query_s<CachedUser>();
}

void UserCacheDao::RemoveUser(const std::string& uid) {
    db_.DB().delete_records_s<CachedUser>("uid=?", uid);
}

// ---- 好友缓存 ----

void UserCacheDao::UpsertFriend(const CachedFriend& f) {
    auto& conn = db_.DB();
    auto existing = conn.query_s<CachedFriend>("uid=?", f.uid);
    if (!existing.empty()) {
        auto updated = existing[0];
        updated.nickname = f.nickname;
        updated.avatar = f.avatar;
        updated.conversation_id = f.conversation_id;
        updated.updated_at = f.updated_at;
        conn.update(updated);
    } else {
        conn.insert(f);
    }
}

std::optional<CachedFriend> UserCacheDao::GetFriend(const std::string& uid) {
    auto res = db_.DB().query_s<CachedFriend>("uid=?", uid);
    if (res.empty()) return std::nullopt;
    return res[0];
}

std::vector<CachedFriend> UserCacheDao::GetAllFriends() {
    return db_.DB().query_s<CachedFriend>();
}

void UserCacheDao::RemoveFriend(const std::string& uid) {
    db_.DB().delete_records_s<CachedFriend>("uid=?", uid);
}

void UserCacheDao::ClearFriends() {
    db_.DB().delete_records_s<CachedFriend>();
}

}  // namespace nova::client
