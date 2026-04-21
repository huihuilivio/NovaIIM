#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <nova/packet.h>

namespace nova {

// 从 nova::proto 引入协议类型，服务端代码可直接使用
using proto::Cmd;
using proto::Packet;
using proto::kHeaderSize;
using proto::kMaxBodySize;

// 连接抽象（对应架构文档 4.1 Gateway - Connection）
// 纯接口，不依赖具体网络库，便于测试 mock
class Connection {
public:
    virtual ~Connection() = default;

    int64_t user_id() const { return user_id_.load(std::memory_order_acquire); }
    void set_user_id(int64_t uid) { user_id_.store(uid, std::memory_order_release); }

    // Snowflake uid（对外暴露的用户标识）
    // 重新登录时可被覆写，需与并发读同步
    std::string uid() const {
        std::lock_guard<std::mutex> lock(uid_mutex_);
        return uid_;
    }
    void set_uid(const std::string& uid) {
        std::lock_guard<std::mutex> lock(uid_mutex_);
        uid_ = uid;
    }

    std::string device_id() const {
        std::lock_guard<std::mutex> lock(device_mutex_);
        return device_id_;
    }
    void set_device_id(const std::string& did) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        device_id_ = did;
    }

    std::string device_type() const {
        std::lock_guard<std::mutex> lock(device_mutex_);
        return device_type_;
    }
    void set_device_type(const std::string& dt) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        device_type_ = dt;
    }

    bool is_authenticated() const { return user_id() != 0; }

    // 发送数据包
    virtual void Send(const Packet& pkt) = 0;

    // 发送已编码的原始帧（用于广播避免重复编码）
    virtual void SendEncoded(const std::string& data) = 0;

    // 关闭连接
    virtual void Close() = 0;

private:
    std::atomic<int64_t> user_id_{0};  // 内部 DB id，仅用于 ConnManager，不对外暴露
    mutable std::mutex uid_mutex_;
    std::string uid_;                   // Snowflake uid，对外暴露的用户标识
    mutable std::mutex device_mutex_;
    std::string device_id_;
    std::string device_type_;
};

using ConnectionPtr = std::shared_ptr<Connection>;

}  // namespace nova
