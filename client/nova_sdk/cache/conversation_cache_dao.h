#pragma once
// ConversationCacheDao — 会话缓存 DAO

#include <cache/cache_db.h>

#include <optional>
#include <string>
#include <vector>

namespace nova::client {

class ConversationCacheDao {
public:
    explicit ConversationCacheDao(CacheDatabase& db) : db_(db) {}

    void Upsert(const CachedConversation& conv);
    std::optional<CachedConversation> Get(int64_t conversation_id);
    std::vector<CachedConversation> GetAll();
    void UpdateUnread(int64_t conversation_id, int64_t count);
    void UpdateLastMsg(int64_t conversation_id,
                       const std::string& sender_uid,
                       const std::string& sender_nickname,
                       const std::string& content,
                       int msg_type, int64_t time);
    void Remove(int64_t conversation_id);
    void Clear();

private:
    CacheDatabase& db_;
};

}  // namespace nova::client
