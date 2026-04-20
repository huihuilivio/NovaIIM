#pragma once
// JsBridge — WebView2 C++ ↔ JS 双向通信桥
//
// C++ → JS: PostEvent()  → chrome.webview 'message' event
// JS → C++: chrome.webview.postMessage(json) → OnWebMessage()

#include <Windows.h>
#include <objbase.h>
#include <wrl.h>
#include <WebView2.h>

#include <atomic>
#include <memory>
#include <string>

namespace nova::client {
class NovaClient;
class AppVM;
class LoginVM;
class ChatVM;
}

namespace nova::desktop {

class JsBridge {
public:
    JsBridge(ICoreWebView2* webview, nova::client::NovaClient* client);
    ~JsBridge();

    /// 注册 WebMessage 监听 + MsgBus 订阅
    void Init();

    /// 向 JS 发送事件: { "event": name, "data": {...} }
    void PostEvent(const std::string& event, const std::string& json_data);

private:
    void OnWebMessage(const std::wstring& json);

    // 操作分发
    void HandleLogin(const std::string& email, const std::string& password);
    void HandleRegister(const std::string& email, const std::string& nickname,
                        const std::string& password);
    void HandleConnect();
    void HandleDisconnect();
    void HandleSendMessage(const std::string& to_uid, const std::string& content);

    // MsgBus 订阅
    void SubscribeEvents();

    ICoreWebView2* webview_ = nullptr;
    nova::client::NovaClient* client_ = nullptr;
    EventRegistrationToken msg_token_ = {};

    // PostEvent / Observer lambda 的生命周期守卫
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    // 缓存 VM 引用，避免悬挂
    std::shared_ptr<nova::client::AppVM>   app_vm_;
    std::shared_ptr<nova::client::LoginVM> login_vm_;
    std::shared_ptr<nova::client::ChatVM>  chat_vm_;
};

}  // namespace nova::desktop
