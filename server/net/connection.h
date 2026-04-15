#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "../model/packet.h"

namespace nova {

// 连接抽象（对应架构文档 4.1 Gateway - Connection）
// 纯接口，不依赖具体网络库，便于测试 mock
class Connection {
public:
    virtual ~Connection() = default;

    int64_t user_id() const { return user_id_; }
    void set_user_id(int64_t uid) { user_id_ = uid; }

    const std::string& device_id() const { return device_id_; }
    void set_device_id(const std::string& did) { device_id_ = did; }

    bool is_authenticated() const { return user_id_ != 0; }

    // 发送数据包
    virtual void Send(const Packet& pkt) = 0;

    // 关闭连接
    virtual void Close() = 0;

private:
    int64_t     user_id_ = 0;
    std::string device_id_;
};

using ConnectionPtr = std::shared_ptr<Connection>;

} // namespace nova
