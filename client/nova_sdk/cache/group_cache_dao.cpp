#include "group_cache_dao.h"

namespace nova::client {

// ---- 群组信息 ----

void GroupCacheDao::UpsertGroup(const CachedGroup& group) {
    auto& conn = db_.DB();
    auto existing = conn.query_s<CachedGroup>("conversation_id=?", group.conversation_id);
    if (!existing.empty()) {
        auto updated = existing[0];
        updated.name = group.name;
        updated.avatar = group.avatar;
        updated.owner_id = group.owner_id;
        updated.notice = group.notice;
        updated.member_count = group.member_count;
        conn.update(updated);
    } else {
        conn.insert(group);
    }
}

std::optional<CachedGroup> GroupCacheDao::GetGroup(int64_t conversation_id) {
    auto res = db_.DB().query_s<CachedGroup>("conversation_id=?", conversation_id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

std::vector<CachedGroup> GroupCacheDao::GetAllGroups() {
    return db_.DB().query_s<CachedGroup>();
}

void GroupCacheDao::RemoveGroup(int64_t conversation_id) {
    db_.DB().delete_records_s<CachedGroup>("conversation_id=?", conversation_id);
    db_.DB().delete_records_s<CachedGroupMember>("conversation_id=?", conversation_id);
}

// ---- 群成员 ----

void GroupCacheDao::UpsertMember(const CachedGroupMember& member) {
    auto& conn = db_.DB();
    auto existing = conn.query_s<CachedGroupMember>(
        "conversation_id=? AND uid=?", member.conversation_id, member.uid);
    if (!existing.empty()) {
        auto updated = existing[0];
        updated.user_id = member.user_id;
        updated.nickname = member.nickname;
        updated.avatar = member.avatar;
        updated.role = member.role;
        conn.update(updated);
    } else {
        conn.insert(member);
    }
}

std::vector<CachedGroupMember> GroupCacheDao::GetMembers(int64_t conversation_id) {
    return db_.DB().query_s<CachedGroupMember>("conversation_id=?", conversation_id);
}

void GroupCacheDao::RemoveMember(int64_t conversation_id, const std::string& uid) {
    db_.DB().delete_records_s<CachedGroupMember>(
        "conversation_id=? AND uid=?", conversation_id, uid);
}

void GroupCacheDao::ClearMembers(int64_t conversation_id) {
    db_.DB().delete_records_s<CachedGroupMember>("conversation_id=?", conversation_id);
}

void GroupCacheDao::Clear() {
    db_.DB().delete_records_s<CachedGroup>();
    db_.DB().delete_records_s<CachedGroupMember>();
}

}  // namespace nova::client
