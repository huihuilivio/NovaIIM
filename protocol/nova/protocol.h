#pragma once
// NovaIIM TCP 协议消息体定义
// 支持三种序列化格式：
//   1. struct_pack — 零拷贝二进制（TCP 帧默认，高性能）
//   2. struct_json — JSON 文本（REST API / 调试 / 客户端本地存储）
//   3. struct_yaml — YAML 文本（配置 / 人类可读日志）
//
// 帧结构不变（18 字节头）:
//   cmd:2 | seq:4 | uid:8 | body_len:4 | body(struct_pack binary)
//
// 每个 Cmd 对应一组 Request / Response 结构体

#include <nova/proto_types.h>

#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <ylt/struct_pack.hpp>
#include <ylt/struct_json/json_reader.h>
#include <ylt/struct_json/json_writer.h>
#include <ylt/struct_yaml/yaml_reader.h>
#include <ylt/struct_yaml/yaml_writer.h>

namespace nova::proto {

// ============================================================
//  通用应答（所有命令共用的错误/成功响应）
// ============================================================
struct RspBase {
    int32_t code = 0;  // 0=成功, >0 错误码
    std::string msg;
};

// ============================================================
//  认证
// ============================================================

// C→S  Cmd::kLogin (0x0001)
struct LoginReq {
    std::string email;  // 登录邮箱
    std::string password;
    std::string device_id;
    std::string device_type;  // "pc", "mobile", "web" 等
};

// S→C  Cmd::kLoginAck (0x0002)
struct LoginAck {
    int32_t code = 0;
    std::string msg;
    std::string uid;  // Snowflake uid（对外用户标识）
    std::string nickname;
    std::string avatar;
};

// S→C  Cmd::kLogout (0x0003)  — 复用 RspBase

// S→C  Cmd::kKickNotify (0x0006) — 复用 RspBase
using KickNotify = RspBase;

// C→S  Cmd::kRegister (0x0004)
struct RegisterReq {
    std::string email;     // 必填，最长 255 字符，格式校验，不区分大小写
    std::string nickname;  // 必填，可重复，最长 100 字符，自动 trim 首尾空白，禁止控制字符
    std::string password;  // 必填，6–128 字符
};

// S→C  Cmd::kRegisterAck (0x0005)
struct RegisterAck {
    int32_t code = 0;
    std::string msg;
    std::string uid;  // 服务端生成的唯一 UID
};

// ============================================================
//  心跳
// ============================================================

// C→S  Cmd::kHeartbeat (0x0010)  — body 可为空
// S→C  Cmd::kHeartbeatAck (0x0011) — 复用 RspBase

// ============================================================
//  消息
// ============================================================

// C→S  Cmd::kSendMsg (0x0100)
struct SendMsgReq {
    int64_t conversation_id = 0;
    std::string content;
    MsgType msg_type = MsgType::kText;
    std::string client_msg_id;  // 客户端生成的去重 ID（可选，用于消息幂等）
};

// S→C  Cmd::kSendMsgAck (0x0101)
struct SendMsgAck {
    int32_t code = 0;
    std::string msg;
    int64_t server_seq  = 0;
    int64_t server_time = 0;  // epoch ms
};

// S→C  Cmd::kPushMsg (0x0102)
struct PushMsg {
    int64_t conversation_id = 0;
    std::string sender_uid;  // 发送者 Snowflake uid
    std::string content;
    int64_t server_seq  = 0;
    int64_t server_time = 0;
    MsgType msg_type    = MsgType::kText;
};

// C→S  Cmd::kRecallMsg (0x0105)
struct RecallMsgReq {
    int64_t conversation_id = 0;
    int64_t server_seq      = 0;
};

// S→C  Cmd::kRecallMsgAck (0x0106)
struct RecallMsgAck {
    int32_t code = 0;
    std::string msg;
};

// S→C  Cmd::kRecallNotify (0x0107)
struct RecallNotify {
    int64_t conversation_id = 0;
    int64_t server_seq      = 0;
    std::string operator_uid;  // 撤回操作者 uid
};

// C→S  Cmd::kDeliverAck (0x0103)
struct DeliverAckReq {
    int64_t conversation_id = 0;
    int64_t server_seq      = 0;
};

// C→S  Cmd::kReadAck (0x0104)
struct ReadAckReq {
    int64_t conversation_id = 0;
    int64_t read_up_to_seq  = 0;
};

// ============================================================
//  同步
// ============================================================

// C→S  Cmd::kSyncMsg (0x0200)
struct SyncMsgReq {
    int64_t conversation_id = 0;
    int64_t last_seq        = 0;
    int32_t limit           = 20;
};

// 同步消息条目（嵌套在 SyncMsgResp 中）
struct SyncMsgItem {
    int64_t server_seq = 0;
    std::string sender_uid;  // 发送者 Snowflake uid
    std::string content;
    MsgType msg_type = MsgType::kText;
    std::string server_time;  // 数据库时间字符串
    MsgStatus status = MsgStatus::kNormal;
};

// S→C  Cmd::kSyncMsgResp (0x0201)
struct SyncMsgResp {
    int32_t code = 0;
    std::vector<SyncMsgItem> messages;
    bool has_more = false;
};

// C→S  Cmd::kSyncUnread (0x0202)  — body 可为空

// 会话未读条目
struct UnreadItem {
    int64_t conversation_id = 0;
    int64_t count           = 0;
    std::vector<SyncMsgItem> latest_messages;
};

// S→C  Cmd::kSyncUnreadResp (0x0203)
struct SyncUnreadResp {
    int32_t code = 0;
    std::vector<UnreadItem> items;
    int64_t total_unread = 0;
};

// ============================================================
//  个人资料
// ============================================================

// C→S  Cmd::kGetUserProfile (0x0302)
struct GetUserProfileReq {
    std::string target_uid;  // 要查询的用户 uid，空表示查自己
};

// S→C  Cmd::kGetUserProfileAck (0x0303)
struct GetUserProfileAck {
    int32_t code = 0;
    std::string msg;
    std::string uid;
    std::string nickname;
    std::string avatar;
    std::string email;  // 仅查自己时返回，查他人为空
};

// ============================================================
//  用户搜索 / 资料编辑
// ============================================================

// C→S  Cmd::kSearchUser (0x0400)
struct SearchUserReq {
    std::string keyword;  // 含 '@' 按邮箱精确匹配，否则按昵称模糊搜索
};

// 搜索结果条目（脱敏：不含 email、password_hash）
struct SearchUserItem {
    std::string uid;
    std::string nickname;
    std::string avatar;
};

// S→C  Cmd::kSearchUserAck (0x0401)
struct SearchUserAck {
    int32_t code = 0;
    std::string msg;
    std::vector<SearchUserItem> users;
};

// C→S  Cmd::kUpdateProfile (0x0402)
struct UpdateProfileReq {
    std::string nickname;   // 空表示不修改
    std::string avatar;     // 空表示不修改
    std::string file_hash;  // 可选，头像文件哈希（仅当 avatar 非空时有意义）
};

// S→C  Cmd::kUpdateProfileAck (0x0403)
struct UpdateProfileAck {
    int32_t code = 0;
    std::string msg;
};

// ============================================================
//  好友
// ============================================================

// C→S  Cmd::kAddFriend (0x0030)
struct AddFriendReq {
    std::string target_uid;  // 目标用户 uid
    std::string remark;      // 验证消息（可选，最长 200 字符）
};

// S→C  Cmd::kAddFriendAck (0x0031)
struct AddFriendAck {
    int32_t code = 0;
    std::string msg;
    int64_t request_id = 0;  // 好友申请 ID
};

// C→S  Cmd::kHandleFriendReq (0x0032)
struct HandleFriendReqReq {
    int64_t request_id = 0;
    int32_t action     = 0;  // 1=accept, 2=reject
};

// S→C  Cmd::kHandleFriendReqAck (0x0033)
struct HandleFriendReqAck {
    int32_t code = 0;
    std::string msg;
    int64_t conversation_id = 0;  // 同意时返回私聊会话 ID
};

// C→S  Cmd::kDeleteFriend (0x0034)
struct DeleteFriendReq {
    std::string target_uid;
};
// S→C  Cmd::kDeleteFriendAck (0x0035)
struct DeleteFriendAck {
    int32_t code = 0;
    std::string msg;
};

// C→S  Cmd::kBlockFriend (0x0036)
struct BlockFriendReq {
    std::string target_uid;
};
// S→C  Cmd::kBlockFriendAck (0x0037)
struct BlockFriendAck {
    int32_t code = 0;
    std::string msg;
};

// C→S  Cmd::kUnblockFriend (0x0038)
struct UnblockFriendReq {
    std::string target_uid;
};
// S→C  Cmd::kUnblockFriendAck (0x0039)
struct UnblockFriendAck {
    int32_t code = 0;
    std::string msg;
};

// 好友列表条目
struct FriendItem {
    std::string uid;
    std::string nickname;
    std::string avatar;
    int64_t conversation_id = 0;  // 对应私聊会话 ID
};

// C→S  Cmd::kGetFriendList (0x003A)  — body 可为空
struct GetFriendListReq {
    int32_t _reserved = 0;  // struct_pack 不允许空结构体
};

// S→C  Cmd::kGetFriendListAck (0x003B)
struct GetFriendListAck {
    int32_t code = 0;
    std::string msg;
    std::vector<FriendItem> friends;
};

// 好友申请条目
struct FriendRequestItem {
    int64_t request_id = 0;
    std::string from_uid;
    std::string from_nickname;
    std::string from_avatar;
    std::string remark;
    std::string created_at;
    int32_t status = 0;  // 0=pending, 1=accepted, 2=rejected
};

// C→S  Cmd::kGetFriendRequests (0x003C)
struct GetFriendRequestsReq {
    int32_t page      = 1;
    int32_t page_size = 20;
};

// S→C  Cmd::kGetFriendRequestsAck (0x003D)
struct GetFriendRequestsAck {
    int32_t code = 0;
    std::string msg;
    std::vector<FriendRequestItem> requests;
    int64_t total = 0;
};

// S→C  Cmd::kFriendNotify (0x003E)
// notify_type: 1=新申请, 2=已同意, 3=已拒绝, 4=已删除
struct FriendNotifyMsg {
    int32_t notify_type = 0;
    std::string from_uid;
    std::string from_nickname;
    std::string from_avatar;
    std::string remark;
    int64_t request_id      = 0;
    int64_t conversation_id = 0;  // 同意时附带私聊会话 ID
};

// ============================================================
//  会话管理
// ============================================================

// C→S  Cmd::kGetConvList (0x0112)
struct GetConvListReq {
    int32_t _reserved = 0;
};

// 最后一条消息摘要
struct LastMsgBrief {
    std::string sender_uid;
    std::string sender_nickname;
    std::string content;
    int32_t msg_type    = 0;
    int64_t server_time = 0;
};

// 会话列表条目
struct ConvItem {
    int64_t conversation_id = 0;
    int32_t type            = 0;  // 1=私聊，2=群聊
    std::string name;             // 私聊=对方昵称，群聊=群名
    std::string avatar;           // 私聊=对方头像，群聊=群头像
    int64_t unread_count = 0;
    LastMsgBrief last_msg;
    int32_t mute   = 0;  // 0=正常，1=免打扰
    int32_t pinned = 0;  // 0=不置顶，1=置顶
    std::string updated_at;
};

// S→C  Cmd::kGetConvListAck (0x0113)
struct GetConvListAck {
    int32_t code = 0;
    std::string msg;
    std::vector<ConvItem> conversations;
};

// C→S  Cmd::kDeleteConv (0x0114)
struct DeleteConvReq {
    int64_t conversation_id = 0;
};

// S→C  Cmd::kDeleteConvAck (0x0115)
struct DeleteConvAck {
    int32_t code = 0;
    std::string msg;
};

// C→S  Cmd::kMuteConv (0x0116)
struct MuteConvReq {
    int64_t conversation_id = 0;
    int32_t mute            = 0;  // 0=取消免打扰, 1=开启免打扰
};

// S→C  Cmd::kMuteConvAck (0x0117)
struct MuteConvAck {
    int32_t code = 0;
    std::string msg;
};

// C→S  Cmd::kPinConv (0x0118)
struct PinConvReq {
    int64_t conversation_id = 0;
    int32_t pinned          = 0;  // 0=取消置顶, 1=置顶
};

// S→C  Cmd::kPinConvAck (0x0119)
struct PinConvAck {
    int32_t code = 0;
    std::string msg;
};

// S→C  Cmd::kConvUpdate (0x011A)
// update_type: 1=新消息, 2=成员变化, 3=会话信息变更, 4=会话解散
struct ConvUpdateMsg {
    int64_t conversation_id = 0;
    int32_t update_type     = 0;
    std::string data;  // JSON 附加数据
};

// ============================================================
//  群组管理
// ============================================================

// C→S  Cmd::kCreateGroup (0x0500)
struct CreateGroupReq {
    std::string name;
    std::string avatar;
    std::vector<int64_t> member_ids;  // 初始成员 ID（不含自己）
};

// S→C  Cmd::kCreateGroupAck (0x0501)
struct CreateGroupAck {
    int32_t code = 0;
    std::string msg;
    int64_t conversation_id = 0;
    int64_t group_id        = 0;
};

// C→S  Cmd::kDismissGroup (0x0502)
struct DismissGroupReq {
    int64_t conversation_id = 0;
};

// S→C  Cmd::kDismissGroupAck (0x0503)
using DismissGroupAck = RspBase;

// C→S  Cmd::kJoinGroup (0x0504)
struct JoinGroupReq {
    int64_t conversation_id = 0;
    std::string remark;
};

// S→C  Cmd::kJoinGroupAck (0x0505)
using JoinGroupAck = RspBase;

// C→S  Cmd::kHandleJoinReq (0x0506)
struct HandleJoinReqReq {
    int64_t request_id = 0;
    int32_t action     = 0;  // 1=accept, 2=reject
};

// S→C  Cmd::kHandleJoinReqAck (0x0507)
using HandleJoinReqAck = RspBase;

// C→S  Cmd::kLeaveGroup (0x0508)
struct LeaveGroupReq {
    int64_t conversation_id = 0;
};

// S→C  Cmd::kLeaveGroupAck (0x0509)
using LeaveGroupAck = RspBase;

// C→S  Cmd::kKickMember (0x050A)
struct KickMemberReq {
    int64_t conversation_id = 0;
    int64_t target_user_id  = 0;
};

// S→C  Cmd::kKickMemberAck (0x050B)
using KickMemberAck = RspBase;

// C→S  Cmd::kGetGroupInfo (0x050C)
struct GetGroupInfoReq {
    int64_t conversation_id = 0;
};

// S→C  Cmd::kGetGroupInfoAck (0x050D)
struct GetGroupInfoAck {
    int32_t code = 0;
    std::string msg;
    int64_t conversation_id = 0;
    std::string name;
    std::string avatar;
    int64_t owner_id = 0;
    std::string notice;
    int32_t member_count = 0;
    std::string created_at;
};

// C→S  Cmd::kUpdateGroup (0x050E)
struct UpdateGroupReq {
    int64_t conversation_id = 0;
    std::string name;    // 空=不修改
    std::string avatar;  // 空=不修改
    std::string notice;  // 空=不修改, 特殊值 "\0" 表示清空
};

// S→C  Cmd::kUpdateGroupAck (0x050F)
using UpdateGroupAck = RspBase;

// 群成员条目
struct GroupMemberItem {
    int64_t user_id = 0;
    std::string uid;
    std::string nickname;
    std::string avatar;
    int32_t role = 0;  // 0=成员, 1=管理员, 2=群主
    std::string joined_at;
};

// C→S  Cmd::kGetGroupMembers (0x0510)
struct GetGroupMembersReq {
    int64_t conversation_id = 0;
};

// S→C  Cmd::kGetGroupMembersAck (0x0511)
struct GetGroupMembersAck {
    int32_t code = 0;
    std::string msg;
    std::vector<GroupMemberItem> members;
};

// 我的群列表条目
struct MyGroupItem {
    int64_t conversation_id = 0;
    std::string name;
    std::string avatar;
    int32_t member_count = 0;
    int32_t my_role      = 0;
};

// C→S  Cmd::kGetMyGroups (0x0512)
struct GetMyGroupsReq {
    int32_t _reserved = 0;
};

// S→C  Cmd::kGetMyGroupsAck (0x0513)
struct GetMyGroupsAck {
    int32_t code = 0;
    std::string msg;
    std::vector<MyGroupItem> groups;
};

// C→S  Cmd::kSetMemberRole (0x0514)
struct SetMemberRoleReq {
    int64_t conversation_id = 0;
    int64_t target_user_id  = 0;
    int32_t role            = 0;  // 0=成员, 1=管理员
};

// S→C  Cmd::kSetMemberRoleAck (0x0515)
using SetMemberRoleAck = RspBase;

// S→C  Cmd::kGroupNotify (0x0516)
struct GroupNotifyMsg {
    int64_t conversation_id = 0;
    int32_t notify_type     = 0;  // 1=创建,2=解散,3=加入,4=退出,5=踢出,6=信息变更,7=角色变更
    int64_t operator_id     = 0;
    std::vector<int64_t> target_ids;
    std::string data;  // JSON 附加数据
};

// ============================================================
//  文件上传/下载
// ============================================================

// C→S  Cmd::kUploadReq (0x0600)
struct UploadReq {
    std::string file_name;
    int64_t file_size = 0;
    std::string mime_type;
    std::string file_hash;  // SHA-256, 秒传去重
    std::string file_type;  // avatar/image/voice/video/file
};

// S→C  Cmd::kUploadAck (0x0601)
struct UploadAckMsg {
    int32_t code = 0;
    std::string msg;
    int64_t file_id = 0;
    std::string upload_url;
    int32_t already_exists = 0;  // 1=秒传命中
};

// C→S  Cmd::kUploadComplete (0x0602)
struct UploadCompleteReq {
    int64_t file_id = 0;
};

// S→C  Cmd::kUploadCompleteAck (0x0603)
struct UploadCompleteAckMsg {
    int32_t code = 0;
    std::string msg;
    std::string file_path;
};

// C→S  Cmd::kDownloadReq (0x0604)
struct DownloadReq {
    int64_t file_id = 0;
    int32_t thumb   = 0;  // 1=缩略图
};

// S→C  Cmd::kDownloadAck (0x0605)
struct DownloadAckMsg {
    int32_t code = 0;
    std::string msg;
    std::string download_url;
    std::string file_name;
    int64_t file_size = 0;
};

// ============================================================
//  序列化 / 反序列化便捷函数
// ============================================================

// --- struct_pack (二进制, TCP 帧默认) ---

// 序列化为 std::string（可直接赋给 Packet::body）
template <typename T>
inline std::string Serialize(const T& obj) {
    auto buf = struct_pack::serialize(obj);
    return std::string(reinterpret_cast<const char*>(buf.data()), buf.size());
}

// 反序列化，失败返回 std::nullopt
template <typename T>
inline std::optional<T> Deserialize(const std::string& data) {
    auto result = struct_pack::deserialize<T>(data.data(), data.size());
    if (!result)
        return std::nullopt;
    return std::move(*result);
}

// --- JSON ---

template <typename T>
inline std::string ToJson(const T& obj) {
    std::string s;
    struct_json::to_json(obj, s);
    return s;
}

template <typename T>
inline std::optional<T> FromJson(const std::string& json) {
    T obj{};
    std::error_code ec;
    struct_json::from_json(obj, json, ec);
    if (ec)
        return std::nullopt;
    return obj;
}

// --- YAML ---

template <typename T>
inline std::string ToYaml(const T& obj) {
    std::string s;
    struct_yaml::to_yaml(obj, s);
    return s;
}

template <typename T>
inline std::optional<T> FromYaml(const std::string& yaml) {
    T obj{};
    std::error_code ec;
    struct_yaml::from_yaml(obj, yaml, ec);
    if (ec)
        return std::nullopt;
    return obj;
}

}  // namespace nova::proto
