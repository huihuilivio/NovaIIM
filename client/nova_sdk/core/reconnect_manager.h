#pragma once
// ReconnectManager — 指数退避自动重连
//
// 策略: 1s → 2s → 4s → 8s → 16s → 30s (cap)
// 连接成功后重置计数器
// 使用 Timer 代替 std::thread，可即时取消

#include <model/client_state.h>
#include <infra/timer.h>


#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>

namespace nova::client {

struct ClientConfig;

class ReconnectManager {
public:
    using ReconnectFunc = std::function<void()>;

    explicit ReconnectManager(const ClientConfig& config);
    ~ReconnectManager();

    ReconnectManager(const ReconnectManager&) = delete;
    ReconnectManager& operator=(const ReconnectManager&) = delete;

    /// 设置重连回调（通常调用 TcpClient::Connect）
    void OnReconnect(ReconnectFunc cb) { reconnect_func_ = std::move(cb); }

    /// 业务状态变化通知
    void OnStateChanged(ClientState state);

    /// 启用/禁用自动重连
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }
    bool IsStopped() const { return stopped_; }

    /// 重置退避计数器（连接成功时调用）
    void Reset();

    /// 停止重连
    void Stop();

private:
    void ScheduleReconnect();
    uint32_t NextDelay();

    // 只保留需要的配置字段
    uint32_t initial_delay_ms_;
    uint32_t max_delay_ms_;
    double   multiplier_;

    ReconnectFunc reconnect_func_;
    Timer timer_;

    std::atomic<bool> enabled_{true};
    std::atomic<bool> stopped_{false};
    std::atomic<uint32_t> current_delay_ms_{0};
    std::atomic<uint32_t> attempt_count_{0};

    mutable std::mutex timer_mu_;
    Timer::TimerID pending_timer_{0};  // 受 timer_mu_ 保护
};

}  // namespace nova::client
