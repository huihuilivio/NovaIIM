#include "reconnect_manager.h"
#include <core/client_config.h>
#include <infra/logger.h>

#include <algorithm>

namespace nova::client {

ReconnectManager::ReconnectManager(const ClientConfig& config)
    : initial_delay_ms_(config.reconnect_initial_ms),
      max_delay_ms_(config.reconnect_max_ms),
      multiplier_(config.reconnect_multiplier),
      current_delay_ms_(config.reconnect_initial_ms),
      attempt_count_(0) {}

ReconnectManager::~ReconnectManager() {
    Stop();
}

void ReconnectManager::OnStateChanged(ClientState state) {
    switch (state) {
        case ClientState::kAuthenticated:
            Reset();
            break;
        case ClientState::kDisconnected:
            if (enabled_ && !stopped_) {
                ScheduleReconnect();
            }
            break;
        default:
            break;
    }
}

void ReconnectManager::Reset() {
    current_delay_ms_.store(initial_delay_ms_);
    attempt_count_.store(0);
}

void ReconnectManager::Stop() {
    stopped_ = true;
    enabled_ = false;
    Timer::TimerID id = 0;
    {
        std::lock_guard<std::mutex> lk(timer_mu_);
        id = pending_timer_;
        pending_timer_ = 0;
    }
    if (id) {
        timer_.KillTimer(id);
    }
}

void ReconnectManager::ScheduleReconnect() {
    if (stopped_) return;

    // 取消上一个 pending timer（防止重复调度）
    Timer::TimerID old_id = 0;
    {
        std::lock_guard<std::mutex> lk(timer_mu_);
        old_id = pending_timer_;
        pending_timer_ = 0;
    }
    if (old_id) {
        timer_.KillTimer(old_id);
    }

    uint32_t delay = NextDelay();
    NOVA_LOG_INFO("Reconnecting in {}ms (attempt #{})", delay, attempt_count_.load());

    Timer::TimerID new_id = timer_.SetTimeout(delay, [this](Timer::TimerID) {
        {
            std::lock_guard<std::mutex> lk(timer_mu_);
            pending_timer_ = 0;
        }
        if (!stopped_ && enabled_ && reconnect_func_) {
            reconnect_func_();
        }
    });
    {
        std::lock_guard<std::mutex> lk(timer_mu_);
        // 若并发的 Stop() 已将 stopped_ 置为 true，则不保存 id（让定时器 fire-and-discard）
        if (!stopped_) {
            pending_timer_ = new_id;
        } else {
            timer_.KillTimer(new_id);
        }
    }
}

uint32_t ReconnectManager::NextDelay() {
    auto delay = current_delay_ms_.load();
    attempt_count_.fetch_add(1);
    current_delay_ms_.store(std::min(
        static_cast<uint32_t>(delay * multiplier_),
        max_delay_ms_));
    return delay;
}

}  // namespace nova::client
