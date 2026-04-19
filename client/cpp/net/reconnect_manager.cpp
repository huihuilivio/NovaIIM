#include "reconnect_manager.h"
#include <core/logger.h>

#include <algorithm>
#include <chrono>

namespace nova::client {

ReconnectManager::ReconnectManager(const ClientConfig& config)
    : config_(config),
      current_delay_ms_(config.reconnect_initial_ms),
      attempt_count_(0) {}

ReconnectManager::~ReconnectManager() {
    Stop();
}

void ReconnectManager::OnStateChanged(ConnectionState state) {
    switch (state) {
        case ConnectionState::kAuthenticated:
            Reset();
            break;
        case ConnectionState::kDisconnected:
            if (enabled_ && !stopped_) {
                ScheduleReconnect();
            }
            break;
        default:
            break;
    }
}

void ReconnectManager::Reset() {
    current_delay_ms_.store(config_.reconnect_initial_ms);
    attempt_count_.store(0);
}

void ReconnectManager::Stop() {
    stopped_ = true;
    enabled_ = false;
    std::lock_guard lock(thread_mutex_);
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

void ReconnectManager::ScheduleReconnect() {
    // 原子 CAS 防止并发进入
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    if (stopped_) { running_ = false; return; }

    uint32_t delay = NextDelay();
    NOVA_LOG_INFO("Reconnecting in {}ms (attempt #{})", delay, attempt_count_.load());

    std::lock_guard lock(thread_mutex_);
    // 等前一个 timer 线程结束
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }

    timer_thread_ = std::thread([this, delay]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        running_ = false;
        if (!stopped_ && enabled_ && reconnect_func_) {
            reconnect_func_();
        }
    });
}

uint32_t ReconnectManager::NextDelay() {
    auto delay = current_delay_ms_.load();
    attempt_count_.fetch_add(1);
    current_delay_ms_.store(std::min(
        static_cast<uint32_t>(delay * config_.reconnect_multiplier),
        config_.reconnect_max_ms));
    return delay;
}

}  // namespace nova::client
