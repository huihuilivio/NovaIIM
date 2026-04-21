#pragma once
// MessageCacheDao — 消息 & 同步状态缓存 DAO

#include <cache/cache_db.h>

#include <optional>
#include <string>
#include <vector>

namespace nova::client {

class MessageCacheDao {
public:
    explicit MessageCacheDao(CacheDatabase& db) : db_(db) {}

    // ---- 消息缓存 ----
    void Insert(const CachedMessage& msg);
    void InsertBatch(const std::vector<CachedMessage>& msgs);
    std::vector<CachedMessage> GetByConversation(int64_t conv_id, int limit = 50, int64_t before_seq = 0);
    std::optional<CachedMessage> GetByClientMsgId(const std::string& client_msg_id);
    void UpdateStatus(int64_t conv_id, int64_t server_seq, int status);
    int64_t GetMaxSeq(int64_t conv_id);
    void DeleteByConversation(int64_t conv_id);
    void Clear();

    // ---- 同步状态 ----
    void SetSyncSeq(int64_t conv_id, int64_t seq);
    int64_t GetSyncSeq(int64_t conv_id);

private:
    CacheDatabase& db_;
};

}  // namespace nova::client
