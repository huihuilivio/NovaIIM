#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace nova {

// 二进制帧格式（小端序）:
// +-------+-------+-------+------------+------+
// | cmd:2 | seq:4 | uid:8 | body_len:4 | body |
// +-------+-------+-------+------------+------+
// 固定头 = 18 字节

constexpr uint32_t kHeaderSize = 18;
constexpr uint32_t kMaxBodySize = 1 * 1024 * 1024; // 1 MB

// 协议包结构
struct Packet {
    uint16_t cmd = 0;
    uint32_t seq = 0;
    uint64_t uid = 0;
    std::string body;

    // 编码为二进制帧
    std::string Encode() const {
        uint32_t body_len = static_cast<uint32_t>(body.size());
        std::string buf(kHeaderSize + body_len, '\0');
        char* p = buf.data();
        std::memcpy(p,      &cmd,      2); p += 2;
        std::memcpy(p,      &seq,      4); p += 4;
        std::memcpy(p,      &uid,      8); p += 8;
        std::memcpy(p,      &body_len, 4); p += 4;
        std::memcpy(p,      body.data(), body_len);
        return buf;
    }

    // 从完整帧解码（data 长度 >= kHeaderSize）
    static bool Decode(const char* data, size_t len, Packet& pkt) {
        if (len < kHeaderSize) return false;
        const char* p = data;
        std::memcpy(&pkt.cmd, p, 2); p += 2;
        std::memcpy(&pkt.seq, p, 4); p += 4;
        std::memcpy(&pkt.uid, p, 8); p += 8;
        uint32_t body_len = 0;
        std::memcpy(&body_len, p, 4); p += 4;
        if (body_len > kMaxBodySize) return false;
        if (len < kHeaderSize + body_len) return false;
        pkt.body.assign(p, body_len);
        return true;
    }
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
