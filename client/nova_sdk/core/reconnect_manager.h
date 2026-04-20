#pragma once
// ReconnectManager — 指数退避自动重连
//
// 策略: 1s → 2s → 4s → 8s → 16s → 30s (cap)
// 连接成功后重置计数器

#include <export.h>
#include <core/client_config.h>
#include <net/connection_state.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace nova::client {

class TcpClient;

class NOVA_SDK_API ReconnectManager {
public:
    using ReconnectFunc = std::function<void()>;

    explicit ReconnectManager(const ClientConfig& config);
    ~ReconnectManager();

    ReconnectManager(const ReconnectManager&) = delete;
    ReconnectManager& operator=(const ReconnectManager&) = delete;

    /// 设置重连回调（通常调用 TcpClient::Connect）
    void OnReconnect(ReconnectFunc cb) { reconnect_func_ = std::move(cb); }

    /// 连接状态变化通知
    void OnStateChanged(ConnectionState state);

    /// 启用/禁用自动重连
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

    /// 重置退避计数器（连接成功时调用）
    void Reset();

    /// 停止重连
    void Stop();

private:
    void ScheduleReconnect();
    uint32_t NextDelay();

    ClientConfig config_;
    ReconnectFunc reconnect_func_;

    std::atomic<bool> enabled_{true};
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<uint32_t> current_delay_ms_{0};
    std::atomic<uint32_t> attempt_count_{0};

    std::thread timer_thread_;
    std::mutex thread_mutex_;
};

}  // namespace nova::client
