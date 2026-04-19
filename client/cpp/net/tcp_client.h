#pragma once
// TcpClient — libhv TCP 客户端封装
//
// 职责：连接管理、帧编解码、心跳发送

#include <core/export.h>
#include <core/client_config.h>
#include <net/connection_state.h>

#include <nova/packet.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <hv/TcpClient.h>
#include <hv/hloop.h>

namespace nova::client {

class NOVA_CLIENT_API TcpClient {
public:
    using PacketCallback  = std::function<void(const nova::proto::Packet&)>;
    using StateCallback   = std::function<void(ConnectionState)>;

    explicit TcpClient(const ClientConfig& config);
    ~TcpClient();

    // 禁止拷贝
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    /// 连接服务器
    void Connect();

    /// 断开连接
    void Disconnect();

    /// 发送协议包
    bool Send(const nova::proto::Packet& pkt);

    /// 发送原始数据
    bool SendRaw(const std::string& data);

    /// 当前连接状态
    ConnectionState GetState() const { return state_.load(); }

    /// 设置收包回调
    void OnPacket(PacketCallback cb) { on_packet_ = std::move(cb); }

    /// 设置状态变化回调
    void OnStateChanged(StateCallback cb) { on_state_ = std::move(cb); }

    /// 获取下一个 seq（原子递增）
    uint32_t NextSeq() { return seq_counter_.fetch_add(1); }

private:
    void SetState(ConnectionState s);
    void StartHeartbeat();
    void StopHeartbeat();

    ClientConfig config_;
    std::unique_ptr<hv::TcpClient> client_;
    hv::EventLoopPtr loop_;

    std::atomic<ConnectionState> state_{ConnectionState::kDisconnected};
    std::atomic<uint32_t> seq_counter_{1};

    PacketCallback on_packet_;
    StateCallback  on_state_;

    htimer_t* heartbeat_timer_ = nullptr;
    std::mutex send_mutex_;
};

}  // namespace nova::client
