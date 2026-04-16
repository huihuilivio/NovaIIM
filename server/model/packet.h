#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

namespace nova {

// 二进制帧格式（小端序）:
// +-------+-------+-------+------------+------+
// | cmd:2 | seq:4 | uid:8 | body_len:4 | body |
// +-------+-------+-------+------------+------+
// 固定头 = 18 字节

constexpr uint32_t kHeaderSize = 18;
constexpr uint32_t kMaxBodySize = 1 * 1024 * 1024; // 1 MB

// ---- 显式小端序读写（跨平台安全） ----
namespace detail {

inline void WriteLE16(char* dst, uint16_t v) {
    auto* p = reinterpret_cast<unsigned char*>(dst);
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
}
inline void WriteLE32(char* dst, uint32_t v) {
    auto* p = reinterpret_cast<unsigned char*>(dst);
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
    p[2] = static_cast<unsigned char>(v >> 16);
    p[3] = static_cast<unsigned char>(v >> 24);
}
inline void WriteLE64(char* dst, uint64_t v) {
    auto* p = reinterpret_cast<unsigned char*>(dst);
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<unsigned char>(v >> (i * 8));
    }
}

inline uint16_t ReadLE16(const char* src) {
    auto* p = reinterpret_cast<const unsigned char*>(src);
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
inline uint32_t ReadLE32(const char* src) {
    auto* p = reinterpret_cast<const unsigned char*>(src);
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}
inline uint64_t ReadLE64(const char* src) {
    auto* p = reinterpret_cast<const unsigned char*>(src);
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(p[i]) << (i * 8);
    }
    return v;
}

} // namespace detail

// 协议包结构
struct Packet {
    uint16_t cmd = 0;
    uint32_t seq = 0;
    uint64_t uid = 0;
    std::string body;

    // 编码为二进制帧（小端序，跨平台安全）
    std::string Encode() const {
        uint32_t body_len = static_cast<uint32_t>(body.size());
        std::string buf(kHeaderSize + body_len, '\0');
        char* p = buf.data();
        detail::WriteLE16(p, cmd);      p += 2;
        detail::WriteLE32(p, seq);      p += 4;
        detail::WriteLE64(p, uid);      p += 8;
        detail::WriteLE32(p, body_len); p += 4;
        std::memcpy(p, body.data(), body_len);
        return buf;
    }

    // 从完整帧解码（小端序，跨平台安全）
    static bool Decode(const char* data, size_t len, Packet& pkt) {
        if (len < kHeaderSize) return false;
        const char* p = data;
        pkt.cmd = detail::ReadLE16(p);  p += 2;
        pkt.seq = detail::ReadLE32(p);  p += 4;
        pkt.uid = detail::ReadLE64(p);  p += 8;
        uint32_t body_len = detail::ReadLE32(p); p += 4;
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
