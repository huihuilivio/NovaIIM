#pragma once
// WsClient — libhv WebSocket 客户端封装（PIMPL）
//
// 纯网络层：连接管理、消息收发
// 支持文本和二进制帧

#include "connection_state.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace nova::client {

class WsClient {
public:
    using MessageCallback = std::function<void(const std::string& msg)>;
    using BinaryCallback  = std::function<void(const void* data, size_t len)>;
    using StateCallback   = std::function<void(ConnectionState)>;

    WsClient();
    ~WsClient();

    WsClient(const WsClient&) = delete;
    WsClient& operator=(const WsClient&) = delete;

    /// 设置请求头（如 Authorization，在 Connect 前调用）
    void SetHeader(const std::string& key, const std::string& value);

    /// 连接 WebSocket 服务器
    void Connect(const std::string& url);

    /// 断开连接
    void Disconnect();

    /// 发送文本消息
    bool Send(const std::string& msg);

    /// 发送二进制消息
    bool SendBinary(const void* data, size_t len);

    /// 当前连接状态
    ConnectionState GetState() const;

    /// 设置文本消息回调
    void OnMessage(MessageCallback cb);

    /// 设置二进制消息回调
    void OnBinary(BinaryCallback cb);

    /// 设置状态变化回调
    void OnStateChanged(StateCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nova::client
