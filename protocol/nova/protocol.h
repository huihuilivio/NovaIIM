#pragma once
// NovaIIM TCP 协议消息体定义
// 使用 ylt struct_pack 零拷贝序列化（C++20 聚合体自动反射，无需宏）
//
// 帧结构不变（18 字节头）:
//   cmd:2 | seq:4 | uid:8 | body_len:4 | body(struct_pack binary)
//
// 每个 Cmd 对应一组 Request / Response 结构体

#include <nova/proto_types.h>

#include <cstdint>
#include <string>
#include <vector>

#include <ylt/struct_pack.hpp>

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
    std::string email;       // 登录邮箱
    std::string password;
    std::string device_id;
    std::string device_type;  // "pc", "mobile", "web" 等
};

// S→C  Cmd::kLoginAck (0x0002)
struct LoginAck {
    int32_t code = 0;
    std::string msg;
    std::string uid;      // Snowflake uid（对外用户标识）
    std::string nickname;
    std::string avatar;
};

// S→C  Cmd::kLogout (0x0003)  — 复用 RspBase

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
    std::string uid;       // 服务端生成的唯一 UID
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
    std::string sender_uid;         // 发送者 Snowflake uid
    std::string content;
    int64_t server_seq  = 0;
    int64_t server_time = 0;
    MsgType msg_type    = MsgType::kText;
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
    std::string sender_uid;   // 发送者 Snowflake uid
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
//  文件 / 个人资料
// ============================================================

// C→S  Cmd::kUpdateAvatar (0x0300)
struct UpdateAvatarReq {
    std::string avatar_path;  // 头像路径或 URL
    std::string file_hash;    // 可选，文件哈希
};

// S→C  Cmd::kUpdateAvatarAck (0x0301)
struct UpdateAvatarAck {
    int32_t code = 0;
    std::string msg;
    std::string avatar_path;  // 更新后的路径（服务端可能做了规范化）
};

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
};

// ============================================================
//  序列化 / 反序列化便捷函数
// ============================================================

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

}  // namespace nova::proto
