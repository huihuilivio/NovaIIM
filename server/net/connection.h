#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include "../model/packet.h"

namespace nova {

// 连接抽象（对应架构文档 4.1 Gateway - Connection）
// 纯接口，不依赖具体网络库，便于测试 mock
class Connection {
public:
    virtual ~Connection() = default;

    int64_t user_id() const { return user_id_.load(std::memory_order_acquire); }
    void set_user_id(int64_t uid) { user_id_.store(uid, std::memory_order_release); }

    std::string device_id() const {
        std::lock_guard<std::mutex> lock(device_mutex_);
        return device_id_;
    }
    void set_device_id(const std::string& did) {
        std::lock_guard<std::mutex> lock(device_mutex_);
        device_id_ = did;
    }

    bool is_authenticated() const { return user_id() != 0; }

    // 发送数据包
    virtual void Send(const Packet& pkt) = 0;

    // 发送已编码的原始帧（用于广播避免重复编码）
    virtual void SendEncoded(const std::string& data) = 0;

    // 关闭连接
    virtual void Close() = 0;

private:
    std::atomic<int64_t> user_id_{0};
    mutable std::mutex device_mutex_;
    std::string device_id_;
};

using ConnectionPtr = std::shared_ptr<Connection>;

}  // namespace nova
