#pragma once
// NovaClient — SDK 公共接口（ViewModel 层）
//
// 平台代码（Desktop / iOS / Android）统一通过 NovaClient 使用 SDK
// 内部封装 ClientContext，对外隐藏协议编解码、包构造等细节
//
// 用法:
//   nova::client::NovaClient client(config);
//   client.Init();
//   client.Connect();
//   client.Login("user@email.com", "password", callback);

#include <export.h>
#include <model/client_config.h>
#include <model/client_state.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace nova::client {

class ClientContext;

/// 登录结果
struct LoginResult {
    bool success = false;
    std::string uid;
    std::string nickname;
    std::string avatar;
    std::string msg;        // 失败原因
};

/// 发送消息结果
struct SendMsgResult {
    bool success = false;
    int64_t server_seq = 0;
    int64_t server_time = 0;
    std::string msg;
};

/// 服务端推送消息
struct ReceivedMessage {
    int64_t conversation_id = 0;
    std::string sender_uid;
    std::string content;
    int64_t server_seq = 0;
    int64_t server_time = 0;
    int msg_type = 0;
};

/// 撤回通知
struct RecallNotification {
    int64_t conversation_id = 0;
    int64_t server_seq = 0;
    std::string operator_uid;
};

class NOVA_SDK_API NovaClient {
public:
    // ---- 回调类型 ----
    using StateCallback   = std::function<void(ClientState)>;
    using LoginCallback   = std::function<void(const LoginResult&)>;
    using SendMsgCallback = std::function<void(const SendMsgResult&)>;
    using MessageCallback = std::function<void(const ReceivedMessage&)>;
    using RecallCallback  = std::function<void(const RecallNotification&)>;

    explicit NovaClient(const ClientConfig& config);
    ~NovaClient();

    NovaClient(const NovaClient&) = delete;
    NovaClient& operator=(const NovaClient&) = delete;

    // ---- 生命周期 ----
    void Init();
    void Shutdown();

    // ---- 连接 ----
    void Connect();
    void Disconnect();
    ClientState GetState() const;

    // ---- 认证 ----
    void Login(const std::string& email, const std::string& password, LoginCallback cb);
    void Logout();
    bool IsLoggedIn() const;
    const std::string& Uid() const;

    // ---- 消息 ----
    void SendTextMessage(int64_t conversation_id, const std::string& content, SendMsgCallback cb = nullptr);

    // ---- 事件监听 ----
    void OnStateChanged(StateCallback cb);
    void OnMessageReceived(MessageCallback cb);
    void OnMessageRecalled(RecallCallback cb);

    // ---- 配置 ----
    const ClientConfig& Config() const;

private:
    std::unique_ptr<ClientContext> ctx_;
};

}  // namespace nova::client
