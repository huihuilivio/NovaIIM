#pragma once
// NovaIIM 协议公共枚举 —— 客户端 / 服务端共用
// 值与数据库 & 协议帧中的整数字段一一对应

#include <cstdint>

namespace nova::proto {

// 消息内容类型（对应 SendMsgReq.msg_type / PushMsg.msg_type / SyncMsgItem.msg_type）
//
// content 字段格式约定：
//   kText     → UTF-8 纯文本，最大 5000 字符
//   kImage    → JSON: {"file_id":123,"width":640,"height":480,"thumb":"base64..."}
//   kVoice    → JSON: {"file_id":123,"duration":15}
//   kVideo    → JSON: {"file_id":123,"duration":30,"width":1280,"height":720,"thumb":"base64..."}
//   kFile     → JSON: {"file_id":123,"file_name":"report.pdf","file_size":102400}
//   kLocation → JSON: {"lat":39.9042,"lng":116.4074,"name":"地点名","addr":"详细地址"}
//   kEmoji    → 短标识符: "emoji:laugh" 或 "sticker:pack1/001"
//   kCard     → JSON: {"uid":"...","nickname":"...","avatar":"..."}
//   kSystem   → UTF-8 系统消息文本（加入/退出/改名等，服务端生成）
//   kCustom   → 客户端自定义，服务端透传
//   ≥100      → 保留给业务扩展，服务端透传
enum class MsgType : int32_t {
    kText     = 1,   // 纯文本
    kImage    = 2,   // 图片
    kVoice    = 3,   // 语音
    kVideo    = 4,   // 视频
    kFile     = 5,   // 文件
    kLocation = 6,   // 位置
    kEmoji    = 7,   // Emoji / 表情包
    kCard     = 8,   // 名片（用户卡片）
    kSystem   = 9,   // 系统消息（服务端生成，客户端只读）
    kCustom   = 10,  // 自定义（客户端定义格式，服务端透传）
};

// 判断 msg_type 是否为合法值（含扩展区间 ≥100）
inline bool IsValidMsgType(int32_t v) {
    return (v >= 1 && v <= 10) || v >= 100;
}

// 消息状态（对应 SyncMsgItem.status）
enum class MsgStatus : int32_t {
    kNormal   = 0,  // 正常
    kRecalled = 1,  // 已撤回
    kDeleted  = 2,  // 已删除
};

// 会话类型
enum class ConvType : int32_t {
    kPrivate = 1,  // 私聊
    kGroup   = 2,  // 群聊
};

}  // namespace nova::proto
