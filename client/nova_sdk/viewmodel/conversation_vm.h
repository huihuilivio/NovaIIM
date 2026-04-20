#pragma once
// ConversationVM — 会话列表 ViewModel

#include <export.h>
#include <viewmodel/observable.h>
#include <viewmodel/types.h>

#include <functional>

namespace nova::client {

class ConversationService;

class NOVA_SDK_API ConversationVM {
public:
    using ConvListCallback   = std::function<void(const ConvListResult&)>;
    using ConvNotifyCallback = std::function<void(const ConvNotification&)>;

    explicit ConversationVM(ConversationService& conv);
    ~ConversationVM();

    void GetConversationList(ConvListCallback cb);
    void DeleteConversation(int64_t conversation_id, ResultCallback cb = nullptr);
    void MuteConversation(int64_t conversation_id, bool mute,
                          ResultCallback cb = nullptr);
    void PinConversation(int64_t conversation_id, bool pinned,
                         ResultCallback cb = nullptr);

    // 事件监听
    void OnUpdated(ConvNotifyCallback cb);

    /// 会话列表（Observable）
    Observable<std::vector<Conversation>>& Conversations() { return conversations_; }

private:
    ConversationService& conv_;
    Observable<std::vector<Conversation>> conversations_;
};

}  // namespace nova::client
