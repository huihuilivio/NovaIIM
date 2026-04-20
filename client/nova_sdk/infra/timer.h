#pragma once
// Timer — libhv TimerThread 封装（PIMPL）
//
// 独立定时器线程，支持多个定时器复用同一线程
// 线程安全：setTimer / killTimer / resetTimer 均可跨线程调用

#include <cstdint>
#include <functional>
#include <memory>

namespace nova::client {

class Timer {
public:
    using TimerID  = uint64_t;
    using Callback = std::function<void(TimerID)>;

    Timer();
    ~Timer();

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    /// 设置定时器
    /// @param timeout_ms  触发间隔（毫秒）
    /// @param cb          回调函数
    /// @param repeat      重复次数（0 = 无限）
    /// @return 定时器 ID
    TimerID SetTimer(int timeout_ms, Callback cb, uint32_t repeat = 0);

    /// 单次延时回调
    TimerID SetTimeout(int timeout_ms, Callback cb);

    /// 无限周期回调
    TimerID SetInterval(int interval_ms, Callback cb);

    /// 停止指定定时器
    void KillTimer(TimerID timer_id);

    /// 重置指定定时器（重新计时）
    /// @param timeout_ms  新的间隔（0 = 保持原间隔）
    void ResetTimer(TimerID timer_id, int timeout_ms = 0);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nova::client
