#pragma once
// SyncService — 消息同步相关服务

#include <viewmodel/types.h>

#include <functional>

namespace nova::client {

class ClientContext;

class SyncService {
public:
    using SyncMsgCallback    = std::function<void(const SyncMsgResult&)>;
    using SyncUnreadCallback = std::function<void(const SyncUnreadResult&)>;

    explicit SyncService(ClientContext& ctx);

    void SyncMessages(int64_t conversation_id, int64_t last_seq, int32_t limit,
                      SyncMsgCallback cb);
    void SyncUnread(SyncUnreadCallback cb);

private:
    ClientContext& ctx_;
};

}  // namespace nova::client
