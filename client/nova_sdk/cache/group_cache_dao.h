#pragma once
// GroupCacheDao — 群组 & 群成员缓存 DAO

#include <cache/cache_db.h>

#include <optional>
#include <string>
#include <vector>

namespace nova::client {

class GroupCacheDao {
public:
    explicit GroupCacheDao(CacheDatabase& db) : db_(db) {}

    // ---- 群组信息 ----
    void UpsertGroup(const CachedGroup& group);
    std::optional<CachedGroup> GetGroup(int64_t conversation_id);
    std::vector<CachedGroup> GetAllGroups();
    void RemoveGroup(int64_t conversation_id);

    // ---- 群成员 ----
    void UpsertMember(const CachedGroupMember& member);
    std::vector<CachedGroupMember> GetMembers(int64_t conversation_id);
    void RemoveMember(int64_t conversation_id, const std::string& uid);
    void ClearMembers(int64_t conversation_id);

    void Clear();

private:
    CacheDatabase& db_;
};

}  // namespace nova::client
