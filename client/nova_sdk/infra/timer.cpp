#include "timer.h"

#include <hv/EventLoopThread.h>

namespace nova::client {

struct Timer::Impl {
    hv::EventLoopThread thread;

    Impl() { thread.start(); }
    ~Impl() { thread.stop(true); }
};

Timer::Timer() : impl_(std::make_unique<Impl>()) {}

Timer::~Timer() = default;

Timer::TimerID Timer::SetTimer(int timeout_ms, Callback cb, uint32_t repeat) {
    uint32_t hv_repeat = (repeat == 0) ? INFINITE : repeat;
    return impl_->thread.loop()->setTimerInLoop(timeout_ms,
        [cb = std::move(cb)](hv::TimerID id) { cb(id); }, hv_repeat);
}

Timer::TimerID Timer::SetTimeout(int timeout_ms, Callback cb) {
    return impl_->thread.loop()->setTimeout(timeout_ms,
        [cb = std::move(cb)](hv::TimerID id) { cb(id); });
}

Timer::TimerID Timer::SetInterval(int interval_ms, Callback cb) {
    return impl_->thread.loop()->setInterval(interval_ms,
        [cb = std::move(cb)](hv::TimerID id) { cb(id); });
}

void Timer::KillTimer(TimerID timer_id) {
    impl_->thread.loop()->killTimer(timer_id);
}

void Timer::ResetTimer(TimerID timer_id, int timeout_ms) {
    impl_->thread.loop()->resetTimer(timer_id, timeout_ms);
}

}  // namespace nova::client
