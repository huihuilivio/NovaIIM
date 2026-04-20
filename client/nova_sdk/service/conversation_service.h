#pragma once
// ConversationService — 会话管理相关服务

#include <viewmodel/types.h>

#include <functional>

namespace nova::client {

class ClientContext;

class ConversationService {
public:
    using ConvListCallback   = std::function<void(const ConvListResult&)>;
    using ConvNotifyCallback = std::function<void(const ConvNotification&)>;

    explicit ConversationService(ClientContext& ctx);

    void GetConversationList(ConvListCallback cb);
    void DeleteConversation(int64_t conversation_id, ResultCallback cb = nullptr);
    void MuteConversation(int64_t conversation_id, bool mute,
                          ResultCallback cb = nullptr);
    void PinConversation(int64_t conversation_id, bool pinned,
                         ResultCallback cb = nullptr);

    // 事件监听
    void OnUpdated(ConvNotifyCallback cb);

private:
    ClientContext& ctx_;
};

}  // namespace nova::client
