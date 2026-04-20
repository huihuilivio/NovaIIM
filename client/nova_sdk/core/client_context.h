#pragma once
// ClientContext — 客户端全局上下文（依赖注入容器）
//
// 持有 TcpClient / RequestManager / ReconnectManager 的生命周期
// 协议编解码、心跳、拆包等业务逻辑在此层处理

#include <core/client_config.h>
#include <model/client_state.h>
#include <core/reconnect_manager.h>
#include <core/request_manager.h>
#include <infra/tcp_client.h>
#include <infra/timer.h>

#include <msgbus/message_bus.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nova::proto { struct Packet; }

namespace nova::client {

class ClientContext {
public:
    explicit ClientContext(const ClientConfig& config);
    ~ClientContext();

    ClientContext(const ClientContext&) = delete;
    ClientContext& operator=(const ClientContext&) = delete;

    /// 初始化所有子系统
    void Init();

    /// 关闭所有子系统
    void Shutdown();

    /// 连接服务器
    void Connect();

    /// 发送协议包（编码后通过 TcpClient 发送）
    bool SendPacket(const nova::proto::Packet& pkt);

    /// 获取下一个 seq（原子递增）
    uint32_t NextSeq() { return seq_counter_.fetch_add(1); }

    // ---- 访问器 ----
    TcpClient&        Network()    { return *tcp_client_; }
    RequestManager&   Requests()   { return *request_mgr_; }
    ReconnectManager& Reconnect()  { return *reconnect_mgr_; }
    msgbus::MessageBus& Events()   { return event_bus_; }
    const ClientConfig& Config() const { return config_; }

    // ---- 业务状态 ----
    ClientState GetState() const { return state_.load(); }
    using StateCallback = std::function<void(ClientState)>;
    void OnStateChanged(StateCallback cb) {
        std::lock_guard lock(state_cb_mutex_);
        state_callbacks_.push_back(std::move(cb));
    }

    // ---- 登录状态 ----
    void SetAuthenticated(const std::string& uid);
    const std::string& Uid() const { return uid_; }
    bool IsLoggedIn() const { return !uid_.empty(); }

private:
    void SetState(ClientState s);
    void SetupPacketDispatch();
    void StartHeartbeat();
    void StopHeartbeat();
    void StartTimeoutChecker();
    void StopTimeoutChecker();

    ClientConfig config_;
    std::atomic<ClientState> state_{ClientState::kDisconnected};
    std::mutex state_cb_mutex_;
    std::vector<StateCallback> state_callbacks_;
    std::unique_ptr<TcpClient> tcp_client_;
    std::unique_ptr<RequestManager> request_mgr_;
    std::unique_ptr<ReconnectManager> reconnect_mgr_;
    msgbus::MessageBus event_bus_;
    std::atomic<uint32_t> seq_counter_{1};
    Timer timer_;
    Timer::TimerID heartbeat_timer_id_ = 0;
    Timer::TimerID timeout_checker_id_ = 0;
    std::string uid_;
};

}  // namespace nova::client
