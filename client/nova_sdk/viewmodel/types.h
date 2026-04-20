#pragma once
// types.h — SDK 公共数据类型
//
// 所有对外可见的 DTO 结构体集中定义在此
// ViewModel / Service / 平台层均可引用

#include <model/client_state.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nova::client {

// ================================================================
//  通用
// ================================================================

struct Result {
    bool success = false;
    std::string msg;
};

using ResultCallback = std::function<void(const Result&)>;

// ================================================================
//  认证
// ================================================================

struct LoginResult {
    bool success = false;
    std::string uid;
    std::string nickname;
    std::string avatar;
    std::string msg;
};

struct RegisterResult {
    bool success = false;
    std::string uid;
    std::string msg;
};

// ================================================================
//  消息
// ================================================================

struct SendMsgResult {
    bool success = false;
    int64_t server_seq = 0;
    int64_t server_time = 0;
    std::string msg;
};

struct ReceivedMessage {
    int64_t conversation_id = 0;
    std::string sender_uid;
    std::string content;
    int64_t server_seq = 0;
    int64_t server_time = 0;
    int msg_type = 0;
};

struct RecallNotification {
    int64_t conversation_id = 0;
    int64_t server_seq = 0;
    std::string operator_uid;
};

// ================================================================
//  同步
// ================================================================

struct SyncMessage {
    int64_t server_seq = 0;
    std::string sender_uid;
    std::string content;
    int msg_type = 0;
    std::string server_time;
    int status = 0;    // 0=normal, 1=recalled, 2=deleted
};

struct SyncMsgResult {
    bool success = false;
    std::vector<SyncMessage> messages;
    bool has_more = false;
};

struct UnreadEntry {
    int64_t conversation_id = 0;
    int64_t count = 0;
    std::vector<SyncMessage> latest_messages;
};

struct SyncUnreadResult {
    bool success = false;
    std::vector<UnreadEntry> items;
    int64_t total_unread = 0;
};

// ================================================================
//  文件
// ================================================================

struct UploadResult {
    bool success = false;
    std::string msg;
    int64_t file_id = 0;
    std::string upload_url;
    bool already_exists = false;
};

struct UploadCompleteResult {
    bool success = false;
    std::string msg;
    std::string file_path;
};

struct DownloadResult {
    bool success = false;
    std::string msg;
    std::string download_url;
    std::string file_name;
    int64_t file_size = 0;
};

// ================================================================
//  用户资料
// ================================================================

struct UserProfile {
    bool success = false;
    std::string msg;
    std::string uid;
    std::string nickname;
    std::string avatar;
    std::string email;
};

struct SearchUserEntry {
    std::string uid;
    std::string nickname;
    std::string avatar;
};

struct SearchUserResult {
    bool success = false;
    std::string msg;
    std::vector<SearchUserEntry> users;
};

// ================================================================
//  好友
// ================================================================

struct AddFriendResult {
    bool success = false;
    std::string msg;
    int64_t request_id = 0;
};

struct HandleFriendResult {
    bool success = false;
    std::string msg;
    int64_t conversation_id = 0;
};

struct FriendEntry {
    std::string uid;
    std::string nickname;
    std::string avatar;
    int64_t conversation_id = 0;
};

struct FriendListResult {
    bool success = false;
    std::string msg;
    std::vector<FriendEntry> friends;
};

struct FriendRequestEntry {
    int64_t request_id = 0;
    std::string from_uid;
    std::string from_nickname;
    std::string from_avatar;
    std::string remark;
    std::string created_at;
    int status = 0;  // 0=pending, 1=accepted, 2=rejected
};

struct FriendRequestsResult {
    bool success = false;
    std::string msg;
    std::vector<FriendRequestEntry> requests;
    int64_t total = 0;
};

struct FriendNotification {
    int notify_type = 0;   // 1=新申请, 2=已同意, 3=已拒绝, 4=已删除
    std::string from_uid;
    std::string from_nickname;
    std::string from_avatar;
    std::string remark;
    int64_t request_id = 0;
    int64_t conversation_id = 0;
};

// ================================================================
//  会话
// ================================================================

struct LastMsg {
    std::string sender_uid;
    std::string sender_nickname;
    std::string content;
    int msg_type = 0;
    int64_t server_time = 0;
};

struct Conversation {
    int64_t conversation_id = 0;
    int type = 0;           // 1=私聊, 2=群聊
    std::string name;
    std::string avatar;
    int64_t unread_count = 0;
    LastMsg last_msg;
    int mute = 0;
    int pinned = 0;
    std::string updated_at;
};

struct ConvListResult {
    bool success = false;
    std::string msg;
    std::vector<Conversation> conversations;
};

struct ConvNotification {
    int64_t conversation_id = 0;
    int update_type = 0;   // 1=新消息, 2=成员变化, 3=信息变更, 4=解散
    std::string data;
};

// ================================================================
//  群组
// ================================================================

struct CreateGroupResult {
    bool success = false;
    std::string msg;
    int64_t conversation_id = 0;
    int64_t group_id = 0;
};

struct GroupInfo {
    bool success = false;
    std::string msg;
    int64_t conversation_id = 0;
    std::string name;
    std::string avatar;
    int64_t owner_id = 0;
    std::string notice;
    int member_count = 0;
    std::string created_at;
};

struct GroupMember {
    int64_t user_id = 0;
    std::string uid;
    std::string nickname;
    std::string avatar;
    int role = 0;          // 0=成员, 1=管理员, 2=群主
    std::string joined_at;
};

struct GroupMembersResult {
    bool success = false;
    std::string msg;
    std::vector<GroupMember> members;
};

struct MyGroup {
    int64_t conversation_id = 0;
    std::string name;
    std::string avatar;
    int member_count = 0;
    int my_role = 0;
};

struct MyGroupsResult {
    bool success = false;
    std::string msg;
    std::vector<MyGroup> groups;
};

struct GroupNotification {
    int64_t conversation_id = 0;
    int notify_type = 0;  // 1=创建,2=解散,3=加入,4=退出,5=踢出,6=信息变更,7=角色变更
    int64_t operator_id = 0;
    std::vector<int64_t> target_ids;
    std::string data;
};

}  // namespace nova::client
