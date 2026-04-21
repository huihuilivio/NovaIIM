#include "js_bridge.h"

#include <viewmodel/nova_client.h>
#include <viewmodel/ui_dispatcher.h>
#include <viewmodel/contact_vm.h>
#include <viewmodel/conversation_vm.h>
#include <viewmodel/group_vm.h>
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
    // 标记已销毁，防止异步 lambda 访问悬挂 this
    alive_->store(false);

    // 清除 Observable 观察者（防止后续回调触发）
    if (app_vm_)  app_vm_->State().ClearObservers();

    if (webview_ && msg_token_.value != 0) {
        webview_->remove_WebMessageReceived(msg_token_);
    }
}

// ---- 初始化 ----

void JsBridge::Init() {
    // 缓存 VM 引用
    app_vm_     = client_->App();
    login_vm_   = client_->Login();
    chat_vm_    = client_->Chat();
    contact_vm_ = client_->Contacts();
    conv_vm_    = client_->Conversations();
    group_vm_   = client_->Groups();

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

    // 回调可能在网络线程触发，WebView2 要求 UI 线程调用
    std::weak_ptr<std::atomic<bool>> weak_alive = alive_;
    nova::client::UIDispatcher::Post([this, weak_alive, js = std::move(js)]() {
        auto alive = weak_alive.lock();
        if (!alive || !alive->load()) return;
        auto wjs = Utf8ToWide(js);
        webview_->ExecuteScript(wjs.c_str(), nullptr);
    });
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

    // ---- 认证 ----
    if (action == "login") {
        HandleLogin(j.value("email", ""), j.value("password", ""));
    } else if (action == "register") {
        HandleRegister(j.value("email", ""), j.value("nickname", ""), j.value("password", ""));
    } else if (action == "logout") {
        HandleLogout();
    } else if (action == "connect") {
        HandleConnect();
    } else if (action == "disconnect") {
        HandleDisconnect();
    }
    // ---- 消息 ----
    else if (action == "sendMessage") {
        HandleSendMessage(j.value("to", ""), j.value("content", ""));
    } else if (action == "recallMessage") {
        HandleRecallMessage(j.value("conversationId", (int64_t)0), j.value("serverSeq", (int64_t)0));
    } else if (action == "sendDeliverAck") {
        HandleSendDeliverAck(j.value("conversationId", (int64_t)0), j.value("serverSeq", (int64_t)0));
    } else if (action == "sendReadAck") {
        HandleSendReadAck(j.value("conversationId", (int64_t)0), j.value("readUpToSeq", (int64_t)0));
    } else if (action == "syncMessages") {
        HandleSyncMessages(j.value("conversationId", (int64_t)0), j.value("lastSeq", (int64_t)0), j.value("limit", 20));
    } else if (action == "syncUnread") {
        HandleSyncUnread();
    }
    // ---- 会话 ----
    else if (action == "getConversationList") {
        HandleGetConversationList();
    } else if (action == "deleteConversation") {
        HandleDeleteConversation(j.value("conversationId", (int64_t)0));
    } else if (action == "muteConversation") {
        HandleMuteConversation(j.value("conversationId", (int64_t)0), j.value("mute", false));
    } else if (action == "pinConversation") {
        HandlePinConversation(j.value("conversationId", (int64_t)0), j.value("pinned", false));
    }
    // ---- 联系人 ----
    else if (action == "addFriend") {
        HandleAddFriend(j.value("targetUid", ""), j.value("remark", ""));
    } else if (action == "handleFriendRequest") {
        HandleHandleFriendRequest(j.value("requestId", (int64_t)0), j.value("action_type", 0));
    } else if (action == "deleteFriend") {
        HandleDeleteFriend(j.value("targetUid", ""));
    } else if (action == "blockFriend") {
        HandleBlockFriend(j.value("targetUid", ""));
    } else if (action == "unblockFriend") {
        HandleUnblockFriend(j.value("targetUid", ""));
    } else if (action == "getFriendList") {
        HandleGetFriendList();
    } else if (action == "getFriendRequests") {
        HandleGetFriendRequests(j.value("page", 1), j.value("pageSize", 20));
    } else if (action == "getUserProfile") {
        HandleGetUserProfile(j.value("targetUid", ""));
    } else if (action == "searchUser") {
        HandleSearchUser(j.value("keyword", ""));
    } else if (action == "updateProfile") {
        HandleUpdateProfile(j.value("nickname", ""), j.value("avatar", ""), j.value("fileHash", ""));
    }
    // ---- 群组 ----
    else if (action == "createGroup") {
        HandleCreateGroup(j.value("name", ""), j.value("avatar", ""),
                          j.value("memberIds", std::vector<int64_t>{}));
    } else if (action == "dismissGroup") {
        HandleDismissGroup(j.value("conversationId", (int64_t)0));
    } else if (action == "joinGroup") {
        HandleJoinGroup(j.value("conversationId", (int64_t)0), j.value("remark", ""));
    } else if (action == "handleJoinRequest") {
        HandleHandleJoinRequest(j.value("requestId", (int64_t)0), j.value("action_type", 0));
    } else if (action == "leaveGroup") {
        HandleLeaveGroup(j.value("conversationId", (int64_t)0));
    } else if (action == "kickMember") {
        HandleKickMember(j.value("conversationId", (int64_t)0), j.value("targetUserId", (int64_t)0));
    } else if (action == "getGroupInfo") {
        HandleGetGroupInfo(j.value("conversationId", (int64_t)0));
    } else if (action == "updateGroup") {
        HandleUpdateGroup(j.value("conversationId", (int64_t)0),
                          j.value("name", ""), j.value("avatar", ""), j.value("notice", ""));
    } else if (action == "getGroupMembers") {
        HandleGetGroupMembers(j.value("conversationId", (int64_t)0));
    } else if (action == "getMyGroups") {
        HandleGetMyGroups();
    } else if (action == "setMemberRole") {
        HandleSetMemberRole(j.value("conversationId", (int64_t)0),
                            j.value("targetUserId", (int64_t)0), j.value("role", 0));
    }
    // ---- 文件 ----
    else if (action == "requestUpload") {
        HandleRequestUpload(j.value("fileName", ""), j.value("fileSize", (int64_t)0),
                            j.value("mimeType", ""), j.value("fileHash", ""), j.value("fileType", ""));
    } else if (action == "uploadComplete") {
        HandleUploadComplete(j.value("fileId", (int64_t)0));
    } else if (action == "requestDownload") {
        HandleRequestDownload(j.value("fileId", (int64_t)0), j.value("thumb", false));
    }
    else {
        NOVA_LOG_WARN("JsBridge: unknown action: {}", action);
    }
}

// ---- 操作处理 ----

void JsBridge::HandleLogin(const std::string& email, const std::string& password) {
    NOVA_LOG_INFO("JsBridge: login request for {}", email);

    login_vm_->Login(email, password,
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

void JsBridge::HandleRegister(const std::string& email, const std::string& nickname,
                              const std::string& password) {
    NOVA_LOG_INFO("JsBridge: register request for {}", email);

    login_vm_->Register(email, nickname, password,
        [this](const nova::client::RegisterResult& result) {
            nlohmann::json data;
            data["success"] = result.success;
            if (result.success) {
                data["uid"] = result.uid;
            } else {
                data["msg"] = result.msg;
            }
            PostEvent("registerResult", data.dump());
        });
}

void JsBridge::HandleConnect() {
    client_->Connect();
}

void JsBridge::HandleDisconnect() {
    client_->Disconnect();
}

void JsBridge::HandleLogout() {
    login_vm_->Logout();
    PostEvent("logoutResult", R"({"success":true})");
}

void JsBridge::HandleSendMessage(const std::string& to_uid, const std::string& content) {
    int64_t conversation_id = 0;
    try {
        conversation_id = std::stoll(to_uid);
    } catch (const std::exception&) {
        NOVA_LOG_WARN("JsBridge: invalid conversation id: {}", to_uid);
        return;
    }

    chat_vm_->SendTextMessage(conversation_id, content,
        [this](const nova::client::SendMsgResult& result) {
            nlohmann::json data;
            data["success"]    = result.success;
            data["serverSeq"]  = result.server_seq;
            data["serverTime"] = result.server_time;
            data["msg"]        = result.msg;
            PostEvent("sendMsgResult", data.dump());
        });
}

// ---- 消息操作 ----

void JsBridge::HandleRecallMessage(int64_t conversation_id, int64_t server_seq) {
    chat_vm_->RecallMessage(conversation_id, server_seq,
        [this](const nova::client::Result& r) {
            nlohmann::json data;
            data["success"] = r.success;
            data["msg"]     = r.msg;
            PostEvent("recallResult", data.dump());
        });
}

void JsBridge::HandleSendDeliverAck(int64_t conversation_id, int64_t server_seq) {
    chat_vm_->SendDeliverAck(conversation_id, server_seq);
}

void JsBridge::HandleSendReadAck(int64_t conversation_id, int64_t read_up_to_seq) {
    chat_vm_->SendReadAck(conversation_id, read_up_to_seq);
}

void JsBridge::HandleSyncMessages(int64_t conversation_id, int64_t last_seq, int32_t limit) {
    chat_vm_->SyncMessages(conversation_id, last_seq, limit,
        [this](const nova::client::SyncMsgResult& r) {
            nlohmann::json data;
            data["success"] = r.success;
            data["hasMore"] = r.has_more;
            nlohmann::json msgs = nlohmann::json::array();
            for (auto& m : r.messages) {
                msgs.push_back({
                    {"serverSeq",  m.server_seq},
                    {"senderUid",  m.sender_uid},
                    {"content",    m.content},
                    {"msgType",    m.msg_type},
                    {"serverTime", m.server_time},
                    {"status",     m.status},
                });
            }
            data["messages"] = msgs;
            PostEvent("syncMessagesResult", data.dump());
        });
}

void JsBridge::HandleSyncUnread() {
    chat_vm_->SyncUnread(
        [this](const nova::client::SyncUnreadResult& r) {
            nlohmann::json data;
            data["success"]     = r.success;
            data["totalUnread"] = r.total_unread;
            nlohmann::json items = nlohmann::json::array();
            for (auto& e : r.items) {
                items.push_back({
                    {"conversationId", e.conversation_id},
                    {"count",          e.count},
                });
            }
            data["items"] = items;
            PostEvent("syncUnreadResult", data.dump());
        });
}

// ---- 会话操作 ----

void JsBridge::HandleGetConversationList() {
    conv_vm_->GetConversationList(
        [this](const nova::client::ConvListResult& r) {
            nlohmann::json data;
            data["success"] = r.success;
            data["msg"]     = r.msg;
            nlohmann::json convs = nlohmann::json::array();
            for (auto& c : r.conversations) {
                nlohmann::json item;
                item["conversationId"] = c.conversation_id;
                item["type"]           = c.type;
                item["name"]           = c.name;
                item["avatar"]         = c.avatar;
                item["unreadCount"]    = c.unread_count;
                item["mute"]           = c.mute;
                item["pinned"]         = c.pinned;
                item["updatedAt"]      = c.updated_at;
                item["lastMsg"] = {
                    {"senderUid",      c.last_msg.sender_uid},
                    {"senderNickname", c.last_msg.sender_nickname},
                    {"content",        c.last_msg.content},
                    {"msgType",        c.last_msg.msg_type},
                    {"serverTime",     c.last_msg.server_time},
                };
                convs.push_back(item);
            }
            data["conversations"] = convs;
            PostEvent("conversationList", data.dump());
        });
}

void JsBridge::HandleDeleteConversation(int64_t conversation_id) {
    conv_vm_->DeleteConversation(conversation_id,
        [this](const nova::client::Result& r) {
            PostEvent("deleteConvResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleMuteConversation(int64_t conversation_id, bool mute) {
    conv_vm_->MuteConversation(conversation_id, mute,
        [this](const nova::client::Result& r) {
            PostEvent("muteConvResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandlePinConversation(int64_t conversation_id, bool pinned) {
    conv_vm_->PinConversation(conversation_id, pinned,
        [this](const nova::client::Result& r) {
            PostEvent("pinConvResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

// ---- 联系人操作 ----

void JsBridge::HandleAddFriend(const std::string& target_uid, const std::string& remark) {
    contact_vm_->AddFriend(target_uid, remark,
        [this](const nova::client::AddFriendResult& r) {
            nlohmann::json data;
            data["success"]   = r.success;
            data["msg"]       = r.msg;
            data["requestId"] = r.request_id;
            PostEvent("addFriendResult", data.dump());
        });
}

void JsBridge::HandleHandleFriendRequest(int64_t request_id, int action) {
    contact_vm_->HandleFriendRequest(request_id, action,
        [this](const nova::client::HandleFriendResult& r) {
            nlohmann::json data;
            data["success"]        = r.success;
            data["msg"]            = r.msg;
            data["conversationId"] = r.conversation_id;
            PostEvent("handleFriendRequestResult", data.dump());
        });
}

void JsBridge::HandleDeleteFriend(const std::string& target_uid) {
    contact_vm_->DeleteFriend(target_uid,
        [this](const nova::client::Result& r) {
            PostEvent("deleteFriendResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleBlockFriend(const std::string& target_uid) {
    contact_vm_->BlockFriend(target_uid,
        [this](const nova::client::Result& r) {
            PostEvent("blockFriendResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleUnblockFriend(const std::string& target_uid) {
    contact_vm_->UnblockFriend(target_uid,
        [this](const nova::client::Result& r) {
            PostEvent("unblockFriendResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleGetFriendList() {
    contact_vm_->GetFriendList(
        [this](const nova::client::FriendListResult& r) {
            nlohmann::json data;
            data["success"] = r.success;
            data["msg"]     = r.msg;
            nlohmann::json arr = nlohmann::json::array();
            for (auto& f : r.friends) {
                arr.push_back({
                    {"uid",            f.uid},
                    {"nickname",       f.nickname},
                    {"avatar",         f.avatar},
                    {"conversationId", f.conversation_id},
                });
            }
            data["friends"] = arr;
            PostEvent("friendList", data.dump());
        });
}

void JsBridge::HandleGetFriendRequests(int page, int page_size) {
    contact_vm_->GetFriendRequests(page, page_size,
        [this](const nova::client::FriendRequestsResult& r) {
            nlohmann::json data;
            data["success"] = r.success;
            data["msg"]     = r.msg;
            data["total"]   = r.total;
            nlohmann::json arr = nlohmann::json::array();
            for (auto& req : r.requests) {
                arr.push_back({
                    {"requestId",    req.request_id},
                    {"fromUid",      req.from_uid},
                    {"fromNickname", req.from_nickname},
                    {"fromAvatar",   req.from_avatar},
                    {"remark",       req.remark},
                    {"createdAt",    req.created_at},
                    {"status",       req.status},
                });
            }
            data["requests"] = arr;
            PostEvent("friendRequests", data.dump());
        });
}

void JsBridge::HandleGetUserProfile(const std::string& target_uid) {
    contact_vm_->GetUserProfile(target_uid,
        [this](const nova::client::UserProfile& p) {
            nlohmann::json data;
            data["success"]  = p.success;
            data["msg"]      = p.msg;
            data["uid"]      = p.uid;
            data["nickname"] = p.nickname;
            data["avatar"]   = p.avatar;
            data["email"]    = p.email;
            PostEvent("userProfile", data.dump());
        });
}

void JsBridge::HandleSearchUser(const std::string& keyword) {
    contact_vm_->SearchUser(keyword,
        [this](const nova::client::SearchUserResult& r) {
            nlohmann::json data;
            data["success"] = r.success;
            data["msg"]     = r.msg;
            nlohmann::json arr = nlohmann::json::array();
            for (auto& u : r.users) {
                arr.push_back({
                    {"uid",      u.uid},
                    {"nickname", u.nickname},
                    {"avatar",   u.avatar},
                });
            }
            data["users"] = arr;
            PostEvent("searchUserResult", data.dump());
        });
}

void JsBridge::HandleUpdateProfile(const std::string& nickname, const std::string& avatar,
                                   const std::string& file_hash) {
    contact_vm_->UpdateProfile(nickname, avatar, file_hash,
        [this](const nova::client::Result& r) {
            PostEvent("updateProfileResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

// ---- 群组操作 ----

void JsBridge::HandleCreateGroup(const std::string& name, const std::string& avatar,
                                 const std::vector<int64_t>& member_ids) {
    group_vm_->CreateGroup(name, avatar, member_ids,
        [this](const nova::client::CreateGroupResult& r) {
            nlohmann::json data;
            data["success"]        = r.success;
            data["msg"]            = r.msg;
            data["conversationId"] = r.conversation_id;
            data["groupId"]        = r.group_id;
            PostEvent("createGroupResult", data.dump());
        });
}

void JsBridge::HandleDismissGroup(int64_t conversation_id) {
    group_vm_->DismissGroup(conversation_id,
        [this](const nova::client::Result& r) {
            PostEvent("dismissGroupResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleJoinGroup(int64_t conversation_id, const std::string& remark) {
    group_vm_->JoinGroup(conversation_id, remark,
        [this](const nova::client::Result& r) {
            PostEvent("joinGroupResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleHandleJoinRequest(int64_t request_id, int action) {
    group_vm_->HandleJoinRequest(request_id, action,
        [this](const nova::client::Result& r) {
            PostEvent("handleJoinRequestResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleLeaveGroup(int64_t conversation_id) {
    group_vm_->LeaveGroup(conversation_id,
        [this](const nova::client::Result& r) {
            PostEvent("leaveGroupResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleKickMember(int64_t conversation_id, int64_t target_user_id) {
    group_vm_->KickMember(conversation_id, target_user_id,
        [this](const nova::client::Result& r) {
            PostEvent("kickMemberResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleGetGroupInfo(int64_t conversation_id) {
    group_vm_->GetGroupInfo(conversation_id,
        [this](const nova::client::GroupInfo& g) {
            nlohmann::json data;
            data["success"]        = g.success;
            data["msg"]            = g.msg;
            data["conversationId"] = g.conversation_id;
            data["name"]           = g.name;
            data["avatar"]         = g.avatar;
            data["ownerId"]        = g.owner_id;
            data["notice"]         = g.notice;
            data["memberCount"]    = g.member_count;
            data["createdAt"]      = g.created_at;
            PostEvent("groupInfo", data.dump());
        });
}

void JsBridge::HandleUpdateGroup(int64_t conversation_id, const std::string& name,
                                 const std::string& avatar, const std::string& notice) {
    group_vm_->UpdateGroup(conversation_id, name, avatar, notice,
        [this](const nova::client::Result& r) {
            PostEvent("updateGroupResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

void JsBridge::HandleGetGroupMembers(int64_t conversation_id) {
    group_vm_->GetGroupMembers(conversation_id,
        [this](const nova::client::GroupMembersResult& r) {
            nlohmann::json data;
            data["success"] = r.success;
            data["msg"]     = r.msg;
            nlohmann::json arr = nlohmann::json::array();
            for (auto& m : r.members) {
                arr.push_back({
                    {"userId",   m.user_id},
                    {"uid",      m.uid},
                    {"nickname", m.nickname},
                    {"avatar",   m.avatar},
                    {"role",     m.role},
                    {"joinedAt", m.joined_at},
                });
            }
            data["members"] = arr;
            PostEvent("groupMembers", data.dump());
        });
}

void JsBridge::HandleGetMyGroups() {
    group_vm_->GetMyGroups(
        [this](const nova::client::MyGroupsResult& r) {
            nlohmann::json data;
            data["success"] = r.success;
            data["msg"]     = r.msg;
            nlohmann::json arr = nlohmann::json::array();
            for (auto& g : r.groups) {
                arr.push_back({
                    {"conversationId", g.conversation_id},
                    {"name",           g.name},
                    {"avatar",         g.avatar},
                    {"memberCount",    g.member_count},
                    {"myRole",         g.my_role},
                });
            }
            data["groups"] = arr;
            PostEvent("myGroups", data.dump());
        });
}

void JsBridge::HandleSetMemberRole(int64_t conversation_id, int64_t target_user_id, int role) {
    group_vm_->SetMemberRole(conversation_id, target_user_id, role,
        [this](const nova::client::Result& r) {
            PostEvent("setMemberRoleResult", nlohmann::json({{"success", r.success}, {"msg", r.msg}}).dump());
        });
}

// ---- 文件操作 ----

void JsBridge::HandleRequestUpload(const std::string& file_name, int64_t file_size,
                                   const std::string& mime_type, const std::string& file_hash,
                                   const std::string& file_type) {
    chat_vm_->RequestUpload(file_name, file_size, mime_type, file_hash, file_type,
        [this](const nova::client::UploadResult& r) {
            nlohmann::json data;
            data["success"]       = r.success;
            data["msg"]           = r.msg;
            data["fileId"]        = r.file_id;
            data["uploadUrl"]     = r.upload_url;
            data["alreadyExists"] = r.already_exists;
            PostEvent("requestUploadResult", data.dump());
        });
}

void JsBridge::HandleUploadComplete(int64_t file_id) {
    chat_vm_->UploadComplete(file_id,
        [this](const nova::client::UploadCompleteResult& r) {
            nlohmann::json data;
            data["success"]  = r.success;
            data["msg"]      = r.msg;
            data["filePath"] = r.file_path;
            PostEvent("uploadCompleteResult", data.dump());
        });
}

void JsBridge::HandleRequestDownload(int64_t file_id, bool thumb) {
    chat_vm_->RequestDownload(file_id, thumb,
        [this](const nova::client::DownloadResult& r) {
            nlohmann::json data;
            data["success"]     = r.success;
            data["msg"]         = r.msg;
            data["downloadUrl"] = r.download_url;
            data["fileName"]    = r.file_name;
            data["fileSize"]    = r.file_size;
            PostEvent("requestDownloadResult", data.dump());
        });
}

// ---- 事件订阅 ----

void JsBridge::SubscribeEvents() {
    app_vm_->State().Observe([this](nova::client::ClientState state) {
        nlohmann::json data;
        data["state"] = nova::client::ClientStateStr(state);
        PostEvent("connectionState", data.dump());
    });

    chat_vm_->OnMessageReceived([this](const nova::client::ReceivedMessage& msg) {
        nlohmann::json data;
        data["conversationId"] = msg.conversation_id;
        data["senderUid"]      = msg.sender_uid;
        data["content"]        = msg.content;
        data["serverSeq"]      = msg.server_seq;
        data["serverTime"]     = msg.server_time;
        data["msgType"]        = msg.msg_type;
        PostEvent("newMessage", data.dump());
    });

    chat_vm_->OnMessageRecalled([this](const nova::client::RecallNotification& n) {
        nlohmann::json data;
        data["conversationId"] = n.conversation_id;
        data["serverSeq"]      = n.server_seq;
        data["operatorUid"]    = n.operator_uid;
        PostEvent("recallNotify", data.dump());
    });

    app_vm_->OnKicked([this](int reason, const std::string& msg) {
        nlohmann::json data;
        data["reason"] = reason;
        data["msg"]    = msg;
        PostEvent("kicked", data.dump());
    });

    // 好友通知
    contact_vm_->OnFriendNotify([this](const nova::client::FriendNotification& n) {
        nlohmann::json data;
        data["notifyType"]      = n.notify_type;
        data["fromUid"]         = n.from_uid;
        data["fromNickname"]    = n.from_nickname;
        data["fromAvatar"]      = n.from_avatar;
        data["remark"]          = n.remark;
        data["requestId"]       = n.request_id;
        data["conversationId"]  = n.conversation_id;
        PostEvent("friendNotify", data.dump());
    });

    // 会话更新通知
    conv_vm_->OnUpdated([this](const nova::client::ConvNotification& n) {
        nlohmann::json data;
        data["conversationId"] = n.conversation_id;
        data["updateType"]     = n.update_type;
        data["data"]           = n.data;
        PostEvent("convUpdate", data.dump());
    });

    // 群组通知
    group_vm_->OnNotify([this](const nova::client::GroupNotification& n) {
        nlohmann::json data;
        data["conversationId"] = n.conversation_id;
        data["notifyType"]     = n.notify_type;
        data["operatorId"]     = n.operator_id;
        data["targetIds"]      = n.target_ids;
        data["data"]           = n.data;
        PostEvent("groupNotify", data.dump());
    });
}

}  // namespace nova::desktop
