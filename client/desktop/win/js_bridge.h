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
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nova::client {
class NovaClient;
class AppVM;
class LoginVM;
class ChatVM;
class ContactVM;
class ConversationVM;
class GroupVM;
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
    void HandleLogout();
    void HandleConnect();
    void HandleDisconnect();
    void HandleSendMessage(const std::string& to_uid, const std::string& content);

    // 消息操作
    void HandleRecallMessage(int64_t conversation_id, int64_t server_seq);
    void HandleSendDeliverAck(int64_t conversation_id, int64_t server_seq);
    void HandleSendReadAck(int64_t conversation_id, int64_t read_up_to_seq);
    void HandleSyncMessages(int64_t conversation_id, int64_t last_seq, int32_t limit);
    void HandleSyncUnread();

    // 会话操作
    void HandleGetConversationList();
    void HandleDeleteConversation(int64_t conversation_id);
    void HandleMuteConversation(int64_t conversation_id, bool mute);
    void HandlePinConversation(int64_t conversation_id, bool pinned);

    // 联系人操作
    void HandleAddFriend(const std::string& target_uid, const std::string& remark);
    void HandleHandleFriendRequest(int64_t request_id, int action);
    void HandleDeleteFriend(const std::string& target_uid);
    void HandleBlockFriend(const std::string& target_uid);
    void HandleUnblockFriend(const std::string& target_uid);
    void HandleGetFriendList();
    void HandleGetFriendRequests(int page, int page_size);
    void HandleGetUserProfile(const std::string& target_uid);
    void HandleSearchUser(const std::string& keyword);
    void HandleUpdateProfile(const std::string& nickname, const std::string& avatar,
                             const std::string& file_hash);

    // 群组操作
    void HandleCreateGroup(const std::string& name, const std::string& avatar,
                           const std::vector<int64_t>& member_ids);
    void HandleDismissGroup(int64_t conversation_id);
    void HandleJoinGroup(int64_t conversation_id, const std::string& remark);
    void HandleHandleJoinRequest(int64_t request_id, int action);
    void HandleLeaveGroup(int64_t conversation_id);
    void HandleKickMember(int64_t conversation_id, int64_t target_user_id);
    void HandleGetGroupInfo(int64_t conversation_id);
    void HandleUpdateGroup(int64_t conversation_id, const std::string& name,
                           const std::string& avatar, const std::string& notice);
    void HandleGetGroupMembers(int64_t conversation_id);
    void HandleGetMyGroups();
    void HandleSetMemberRole(int64_t conversation_id, int64_t target_user_id, int role);

    // 文件操作
    void HandleRequestUpload(const std::string& file_name, int64_t file_size,
                             const std::string& mime_type, const std::string& file_hash,
                             const std::string& file_type);
    void HandleUploadComplete(int64_t file_id);
    void HandleRequestDownload(int64_t file_id, bool thumb);

    // MsgBus 订阅
    void SubscribeEvents();

    ICoreWebView2* webview_ = nullptr;
    nova::client::NovaClient* client_ = nullptr;
    EventRegistrationToken msg_token_ = {};

    // PostEvent / Observer lambda 的生命周期守卫
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    // 缓存 VM 引用，避免悬挂
    std::shared_ptr<nova::client::AppVM>          app_vm_;
    std::shared_ptr<nova::client::LoginVM>        login_vm_;
    std::shared_ptr<nova::client::ChatVM>         chat_vm_;
    std::shared_ptr<nova::client::ContactVM>      contact_vm_;
    std::shared_ptr<nova::client::ConversationVM> conv_vm_;
    std::shared_ptr<nova::client::GroupVM>        group_vm_;
};

}  // namespace nova::desktop
