#pragma once

#include <cstdint>
#include <string>

namespace nova {

// 协议包结构
struct Packet {
    uint16_t cmd = 0;
    uint32_t seq = 0;
    uint64_t uid = 0;
    std::string body;
};

// 命令字定义
enum class Cmd : uint16_t {
    // 认证
    kLogin          = 0x0001,
    kLoginAck       = 0x0002,
    kLogout         = 0x0003,

    // 心跳
    kHeartbeat      = 0x0010,
    kHeartbeatAck   = 0x0011,

    // 消息
    kSendMsg        = 0x0100,
    kSendMsgAck     = 0x0101,
    kPushMsg        = 0x0102,
    kDeliverAck     = 0x0103,
    kReadAck        = 0x0104,

    // 同步
    kSyncMsg        = 0x0200,
    kSyncMsgResp    = 0x0201,
    kSyncUnread     = 0x0202,
    kSyncUnreadResp = 0x0203,
};

} // namespace nova
