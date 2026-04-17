#pragma once
// NovaIIM 协议公共枚举 —— 客户端 / 服务端共用
// 值与数据库 & 协议帧中的整数字段一一对应

#include <cstdint>

namespace nova::proto {

// 消息内容类型（对应 SendMsgReq.msg_type / PushMsg.msg_type / SyncMsgItem.msg_type）
enum class MsgType : int32_t {
    kText  = 1,  // 纯文本
    kImage = 2,  // 图片
    kVoice = 3,  // 语音
    kVideo = 4,  // 视频
    kFile  = 5,  // 文件
};

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
