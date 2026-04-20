#include "js_bridge.h"

#include <viewmodel/nova_client.h>
#include <infra/logger.h>
#include <infra/connection_state.h>

#include <nova/packet.h>
#include <nova/protocol.h>

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

JsBridge::JsBridge(ICoreWebView2* webview, nova::client::ClientContext* ctx)
    : webview_(webview), ctx_(ctx) {}

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

    nova::proto::LoginReq req;
    req.email       = email;
    req.password    = password;
    req.device_id   = ctx_->Config().device_id;
    req.device_type = ctx_->Config().device_type;

    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(nova::proto::Cmd::kLogin);
    pkt.seq = ctx_->NextSeq();
    pkt.body = nova::proto::Serialize(req);
    if (pkt.body.empty()) {
        NOVA_LOG_ERROR("JsBridge: failed to serialize LoginReq");
        nlohmann::json data;
        data["success"] = false;
        data["msg"]     = "serialize error";
        PostEvent("loginResult", data.dump());
        return;
    }

    ctx_->Requests().AddPending(pkt.seq,
        [this](const nova::proto::Packet& resp) {
            auto ack = nova::proto::Deserialize<nova::proto::LoginAck>(resp.body);
            nlohmann::json data;
            if (ack && ack->code == 0) {
                ctx_->SetAuthenticated(ack->uid);
                data["success"]  = true;
                data["uid"]      = ack->uid;
                data["nickname"] = ack->nickname;
            } else {
                data["success"] = false;
                data["msg"]     = ack ? ack->msg : "deserialize error";
            }
            PostEvent("loginResult", data.dump());
        },
        [this](uint32_t /*seq*/) {
            nlohmann::json data;
            data["success"] = false;
            data["msg"]     = "login timeout";
            PostEvent("loginResult", data.dump());
        }
    );

    ctx_->SendPacket(pkt);
}

void JsBridge::HandleConnect() {
    ctx_->Connect();
}

void JsBridge::HandleDisconnect() {
    ctx_->Network().Disconnect();
}

void JsBridge::HandleSendMessage(const std::string& to_uid, const std::string& content) {
    nova::proto::SendMsgReq req;
    try {
        req.conversation_id = std::stoll(to_uid);
    } catch (const std::exception&) {
        NOVA_LOG_WARN("JsBridge: invalid conversation id: {}", to_uid);
        return;
    }
    req.content         = content;
    req.msg_type        = nova::proto::MsgType::kText;

    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(nova::proto::Cmd::kSendMsg);
    pkt.seq = ctx_->NextSeq();
    pkt.body = nova::proto::Serialize(req);

    ctx_->Requests().AddPending(pkt.seq,
        [this](const nova::proto::Packet& resp) {
            auto ack = nova::proto::Deserialize<nova::proto::SendMsgAck>(resp.body);
            nlohmann::json data;
            data["success"] = ack && ack->code == 0;
            if (ack) {
                data["serverSeq"]  = ack->server_seq;
                data["serverTime"] = ack->server_time;
                data["msg"]        = ack->msg;
            }
            PostEvent("sendMsgResult", data.dump());
        }
    );

    ctx_->SendPacket(pkt);
}

// ---- MsgBus 订阅 ----

void JsBridge::SubscribeEvents() {
    auto& bus = ctx_->Events();

    // 连接状态变化
    ctx_->Network().OnStateChanged([this](nova::client::ConnectionState state) {
        nlohmann::json data;
        data["state"] = nova::client::ConnectionStateStr(state);
        PostEvent("connectionState", data.dump());
    });

    // 新消息推送
    bus.subscribe<nova::proto::PushMsg>("PushMsg", [this](const nova::proto::PushMsg& msg) {
        nlohmann::json data;
        data["conversationId"] = msg.conversation_id;
        data["senderUid"]      = msg.sender_uid;
        data["content"]        = msg.content;
        data["serverSeq"]      = msg.server_seq;
        data["serverTime"]     = msg.server_time;
        data["msgType"]        = static_cast<int>(msg.msg_type);
        PostEvent("newMessage", data.dump());
    });

    // 消息撤回通知
    bus.subscribe<nova::proto::RecallNotify>("RecallNotify", [this](const nova::proto::RecallNotify& n) {
        nlohmann::json data;
        data["conversationId"] = n.conversation_id;
        data["serverSeq"]      = n.server_seq;
        data["operatorUid"]    = n.operator_uid;
        PostEvent("recallNotify", data.dump());
    });
}

}  // namespace nova::desktop
