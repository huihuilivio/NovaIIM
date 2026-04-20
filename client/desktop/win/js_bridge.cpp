#include "js_bridge.h"

#include <viewmodel/nova_client.h>
#include <infra/logger.h>

#include <hv/json.hpp>
#include <wrl/event.h>

#include <codecvt>
#include <locale>

using namespace Microsoft::WRL;

namespace nova::desktop {

// ---- UTF-8 ↔ UTF-16 工具 ----
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int size = static_cast<int>(s.size());
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), size, nullptr, 0);
    if (len <= 0) return {};
    std::wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), size, ws.data(), len);
    return ws;
}

static std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int size = static_cast<int>(ws.size());
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), size, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), size, s.data(), len, nullptr, nullptr);
    return s;
}

// ---- 构造 / 析构 ----

JsBridge::JsBridge(ICoreWebView2* webview, nova::client::NovaClient* client)
    : webview_(webview), client_(client) {}

JsBridge::~JsBridge() {
    if (webview_ && msg_token_.value != 0) {
        webview_->remove_WebMessageReceived(msg_token_);
    }
}

// ---- 初始化 ----

void JsBridge::Init() {
    // 监听 JS → C++ 消息
    webview_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* /*sender*/,
                   ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                wchar_t* raw = nullptr;
                args->TryGetWebMessageAsString(&raw);
                if (raw) {
                    std::wstring msg(raw);
                    CoTaskMemFree(raw);
                    OnWebMessage(msg);
                }
                return S_OK;
            }).Get(),
        &msg_token_
    );

    // 订阅 MsgBus 事件
    SubscribeEvents();

    NOVA_LOG_INFO("JsBridge initialized");
}

// ---- C++ → JS ----

void JsBridge::PostEvent(const std::string& event, const std::string& json_data) {
    nlohmann::json j;
    j["event"] = event;
    j["data"]  = nlohmann::json::parse(json_data, nullptr, false);
    if (j["data"].is_discarded()) {
        j["data"] = json_data;
    }

    auto js = "window.__novaBridge&&window.__novaBridge.onEvent("
              + j.dump() + ")";
    auto wjs = Utf8ToWide(js);
    webview_->ExecuteScript(wjs.c_str(), nullptr);
}

// ---- JS → C++ ----

void JsBridge::OnWebMessage(const std::wstring& raw) {
    auto utf8 = WideToUtf8(raw);
    NOVA_LOG_DEBUG("JsBridge recv: {}", utf8);

    auto j = nlohmann::json::parse(utf8, nullptr, false);
    if (j.is_discarded() || !j.contains("action")) {
        NOVA_LOG_WARN("JsBridge: invalid message: {}", utf8);
        return;
    }

    auto action = j["action"].get<std::string>();

    if (action == "login") {
        HandleLogin(
            j.value("email", ""),
            j.value("password", "")
        );
    } else if (action == "connect") {
        HandleConnect();
    } else if (action == "disconnect") {
        HandleDisconnect();
    } else if (action == "sendMessage") {
        HandleSendMessage(
            j.value("to", ""),
            j.value("content", "")
        );
    } else {
        NOVA_LOG_WARN("JsBridge: unknown action: {}", action);
    }
}

// ---- 操作处理 ----

void JsBridge::HandleLogin(const std::string& email, const std::string& password) {
    NOVA_LOG_INFO("JsBridge: login request for {}", email);

    client_->Login()->Login(email, password,
        [this](const nova::client::LoginResult& result) {
            nlohmann::json data;
            data["success"]  = result.success;
            if (result.success) {
                data["uid"]      = result.uid;
                data["nickname"] = result.nickname;
            } else {
                data["msg"] = result.msg;
            }
            PostEvent("loginResult", data.dump());
        });
}

void JsBridge::HandleConnect() {
    client_->Connect();
}

void JsBridge::HandleDisconnect() {
    client_->Disconnect();
}

void JsBridge::HandleSendMessage(const std::string& to_uid, const std::string& content) {
    int64_t conversation_id = 0;
    try {
        conversation_id = std::stoll(to_uid);
    } catch (const std::exception&) {
        NOVA_LOG_WARN("JsBridge: invalid conversation id: {}", to_uid);
        return;
    }

    client_->Chat()->SendTextMessage(conversation_id, content,
        [this](const nova::client::SendMsgResult& result) {
            nlohmann::json data;
            data["success"]    = result.success;
            data["serverSeq"]  = result.server_seq;
            data["serverTime"] = result.server_time;
            data["msg"]        = result.msg;
            PostEvent("sendMsgResult", data.dump());
        });
}

// ---- 事件订阅 ----

void JsBridge::SubscribeEvents() {
    client_->App()->State().Observe([this](nova::client::ClientState state) {
        nlohmann::json data;
        data["state"] = nova::client::ClientStateStr(state);
        PostEvent("connectionState", data.dump());
    });

    client_->Chat()->OnMessageReceived([this](const nova::client::ReceivedMessage& msg) {
        nlohmann::json data;
        data["conversationId"] = msg.conversation_id;
        data["senderUid"]      = msg.sender_uid;
        data["content"]        = msg.content;
        data["serverSeq"]      = msg.server_seq;
        data["serverTime"]     = msg.server_time;
        data["msgType"]        = msg.msg_type;
        PostEvent("newMessage", data.dump());
    });

    client_->Chat()->OnMessageRecalled([this](const nova::client::RecallNotification& n) {
        nlohmann::json data;
        data["conversationId"] = n.conversation_id;
        data["serverSeq"]      = n.server_seq;
        data["operatorUid"]    = n.operator_uid;
        PostEvent("recallNotify", data.dump());
    });
}

}  // namespace nova::desktop
