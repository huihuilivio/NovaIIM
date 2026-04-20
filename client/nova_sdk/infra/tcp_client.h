#pragma once
// TcpClient — libhv TCP 客户端封装（PIMPL）
//
// 纯网络层：连接管理、帧拆包、数据收发
// 不包含业务逻辑（心跳、协议编解码等由上层处理）

#include "connection_state.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace nova::client {

class TcpClient {
public:
    using DataCallback  = std::function<void(const void* data, size_t len)>;
    using StateCallback = std::function<void(ConnectionState)>;

    TcpClient();
    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    /// 设置按长度字段拆包（在 Connect 前调用）
    /// @param package_max_length  单包最大长度
    /// @param body_offset         包头长度（body 起始偏移）
    /// @param length_field_offset 长度字段在包头中的偏移
    /// @param length_field_bytes  长度字段占几字节（1/2/4）
    /// @param length_adjustment   长度调整值（length_field 存的是 body 长度时为 head_len）
    /// @param little_endian       长度字段是否小端序
    void SetLengthFieldUnpack(uint32_t package_max_length,
                              uint16_t body_offset,
                              uint16_t length_field_offset,
                              uint16_t length_field_bytes,
                              int16_t  length_adjustment,
                              bool     little_endian = true);

    /// 连接服务器
    void Connect(const std::string& host, uint16_t port);

    /// 断开连接
    void Disconnect();

    /// 发送原始数据
    bool Send(const void* data, size_t len);
    bool Send(const std::string& data);

    /// 当前连接状态
    ConnectionState GetState() const;

    /// 设置收数据回调（拆包后的完整帧）
    void OnData(DataCallback cb);

    /// 设置状态变化回调
    void OnStateChanged(StateCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nova::client
