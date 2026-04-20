#pragma once
// MessageService — 消息收发相关服务

#include <viewmodel/types.h>

#include <functional>
#include <string>

namespace nova::client {

class ClientContext;

class MessageService {
public:
    using SendMsgCallback  = std::function<void(const SendMsgResult&)>;
    using MessageCallback  = std::function<void(const ReceivedMessage&)>;
    using RecallCallback   = std::function<void(const RecallNotification&)>;

    explicit MessageService(ClientContext& ctx);

    void SendTextMessage(int64_t conversation_id, const std::string& content,
                         SendMsgCallback cb = nullptr);
    void RecallMessage(int64_t conversation_id, int64_t server_seq,
                       ResultCallback cb = nullptr);
    void SendDeliverAck(int64_t conversation_id, int64_t server_seq);
    void SendReadAck(int64_t conversation_id, int64_t read_up_to_seq);

    // 事件监听
    void OnReceived(MessageCallback cb);
    void OnRecalled(RecallCallback cb);

private:
    ClientContext& ctx_;
};

}  // namespace nova::client
