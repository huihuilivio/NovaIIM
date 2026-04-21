#include "conversation_cache_dao.h"

namespace nova::client {

void ConversationCacheDao::Upsert(const CachedConversation& conv) {
    auto& conn = db_.DB();
    auto existing = conn.query_s<CachedConversation>("conversation_id=?", conv.conversation_id);
    if (!existing.empty()) {
        auto updated = existing[0];
        updated.type = conv.type;
        updated.name = conv.name;
        updated.avatar = conv.avatar;
        updated.unread_count = conv.unread_count;
        updated.mute = conv.mute;
        updated.pinned = conv.pinned;
        updated.last_msg_sender_uid = conv.last_msg_sender_uid;
        updated.last_msg_sender_nickname = conv.last_msg_sender_nickname;
        updated.last_msg_content = conv.last_msg_content;
        updated.last_msg_type = conv.last_msg_type;
        updated.last_msg_time = conv.last_msg_time;
        updated.updated_at = conv.updated_at;
        conn.update(updated);
    } else {
        conn.insert(conv);
    }
}

std::optional<CachedConversation> ConversationCacheDao::Get(int64_t conversation_id) {
    auto res = db_.DB().query_s<CachedConversation>("conversation_id=?", conversation_id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

std::vector<CachedConversation> ConversationCacheDao::GetAll() {
    return db_.DB().query_s<CachedConversation>();
}

void ConversationCacheDao::UpdateUnread(int64_t conversation_id, int64_t count) {
    auto res = db_.DB().query_s<CachedConversation>("conversation_id=?", conversation_id);
    if (!res.empty()) {
        auto conv = res[0];
        conv.unread_count = count;
        db_.DB().update(conv);
    }
}

void ConversationCacheDao::UpdateLastMsg(int64_t conversation_id,
                                          const std::string& sender_uid,
                                          const std::string& sender_nickname,
                                          const std::string& content,
                                          int msg_type, int64_t time) {
    auto res = db_.DB().query_s<CachedConversation>("conversation_id=?", conversation_id);
    if (!res.empty()) {
        auto conv = res[0];
        conv.last_msg_sender_uid = sender_uid;
        conv.last_msg_sender_nickname = sender_nickname;
        conv.last_msg_content = content;
        conv.last_msg_type = msg_type;
        conv.last_msg_time = time;
        db_.DB().update(conv);
    }
}

void ConversationCacheDao::Remove(int64_t conversation_id) {
    db_.DB().delete_records_s<CachedConversation>("conversation_id=?", conversation_id);
}

void ConversationCacheDao::Clear() {
    db_.DB().delete_records_s<CachedConversation>();
}

}  // namespace nova::client
