#pragma once
// JsBridge — WebView2 C++ ↔ JS 双向通信桥
//
// C++ → JS: PostEvent()  → chrome.webview 'message' event
// JS → C++: chrome.webview.postMessage(json) → OnWebMessage()

#include <Windows.h>
#include <objbase.h>
#include <wrl.h>
#include <WebView2.h>

#include <string>

namespace nova::client { class ClientContext; }

namespace nova::desktop {

class JsBridge {
public:
    JsBridge(ICoreWebView2* webview, nova::client::ClientContext* ctx);
    ~JsBridge();

    /// 注册 WebMessage 监听 + EventBus 订阅
    void Init();

    /// 向 JS 发送事件: { "event": name, "data": {...} }
    void PostEvent(const std::string& event, const std::string& json_data);

private:
    void OnWebMessage(const std::wstring& json);

    // 操作分发
    void HandleLogin(const std::string& email, const std::string& password);
    void HandleConnect();
    void HandleDisconnect();
    void HandleSendMessage(const std::string& to_uid, const std::string& content);

    // EventBus 订阅
    void SubscribeEvents();

    ICoreWebView2* webview_ = nullptr;
    nova::client::ClientContext* ctx_ = nullptr;
    EventRegistrationToken msg_token_ = {};
};

}  // namespace nova::desktop
