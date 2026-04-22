#include "message_cache_dao.h"

#include <infra/logger.h>

namespace nova::client {

// ---- 消息缓存 ----

void MessageCacheDao::Insert(const CachedMessage& msg) {
    db_.DB().insert(msg);
}

void MessageCacheDao::InsertBatch(const std::vector<CachedMessage>& msgs) {
    if (msgs.empty()) return;
    auto& conn = db_.DB();
    conn.execute("BEGIN");
    try {
        for (auto& m : msgs) {
            conn.insert(m);
        }
        conn.execute("COMMIT");
    } catch (const std::exception& e) {
        conn.execute("ROLLBACK");
        NOVA_LOG_ERROR("MessageCacheDao::InsertBatch failed, rolled back: {}", e.what());
    } catch (...) {
        conn.execute("ROLLBACK");
        NOVA_LOG_ERROR("MessageCacheDao::InsertBatch failed (unknown), rolled back");
    }
}

std::vector<CachedMessage> MessageCacheDao::GetByConversation(
    int64_t conv_id, int limit, int64_t before_seq) {
    if (before_seq > 0) {
        return db_.DB().query_s<CachedMessage>(
            "conversation_id=? AND server_seq<? ORDER BY server_seq DESC LIMIT ?",
            conv_id, before_seq, limit);
    }
    return db_.DB().query_s<CachedMessage>(
        "conversation_id=? ORDER BY server_seq DESC LIMIT ?",
        conv_id, limit);
}

std::optional<CachedMessage> MessageCacheDao::GetByClientMsgId(const std::string& client_msg_id) {
    auto res = db_.DB().query_s<CachedMessage>("client_msg_id=?", client_msg_id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

void MessageCacheDao::UpdateStatus(int64_t conv_id, int64_t server_seq, int status) {
    auto res = db_.DB().query_s<CachedMessage>(
        "conversation_id=? AND server_seq=?", conv_id, server_seq);
    if (!res.empty()) {
        auto& msg = res[0];
        msg.status = status;
        db_.DB().update(msg);
    }
}

int64_t MessageCacheDao::GetMaxSeq(int64_t conv_id) {
    auto res = db_.DB().query_s<std::tuple<int64_t>>(
        "SELECT COALESCE(MAX(server_seq),0) FROM cached_messages WHERE conversation_id=?",
        conv_id);
    if (!res.empty()) return std::get<0>(res[0]);
    return 0;
}

void MessageCacheDao::DeleteByConversation(int64_t conv_id) {
    db_.DB().delete_records_s<CachedMessage>(
        "conversation_id=?", conv_id);
}

void MessageCacheDao::Clear() {
    db_.DB().delete_records_s<CachedMessage>();
    db_.DB().delete_records_s<SyncState>();
}

// ---- 同步状态 ----

void MessageCacheDao::SetSyncSeq(int64_t conv_id, int64_t seq) {
    auto& conn = db_.DB();
    auto existing = conn.query_s<SyncState>("conversation_id=?", conv_id);
    if (!existing.empty()) {
        auto state = existing[0];
        state.last_sync_seq = seq;
        conn.update(state);
    } else {
        SyncState state;
        state.conversation_id = conv_id;
        state.last_sync_seq = seq;
        conn.insert(state);
    }
}

int64_t MessageCacheDao::GetSyncSeq(int64_t conv_id) {
    auto res = db_.DB().query_s<SyncState>("conversation_id=?", conv_id);
    if (!res.empty()) return res[0].last_sync_seq;
    return 0;
}

}  // namespace nova::client
