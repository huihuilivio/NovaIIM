#pragma once
// UserCacheDao — 用户信息 & 好友缓存 DAO

#include <cache/cache_db.h>

#include <optional>
#include <string>
#include <vector>

namespace nova::client {

class UserCacheDao {
public:
    explicit UserCacheDao(CacheDatabase& db) : db_(db) {}

    // ---- 用户缓存 ----
    void UpsertUser(const CachedUser& user);
    std::optional<CachedUser> GetUser(const std::string& uid);
    std::vector<CachedUser> GetAllUsers();
    void RemoveUser(const std::string& uid);

    // ---- 好友缓存 ----
    void UpsertFriend(const CachedFriend& f);
    std::optional<CachedFriend> GetFriend(const std::string& uid);
    std::vector<CachedFriend> GetAllFriends();
    void RemoveFriend(const std::string& uid);
    void ClearFriends();

private:
    CacheDatabase& db_;
};

}  // namespace nova::client
