#include "ui_dispatcher.h"

#include <infra/logger.h>

namespace nova::client {

UIDispatcher::DispatchFunc UIDispatcher::dispatcher_;
std::mutex UIDispatcher::mutex_;

void UIDispatcher::Set(DispatchFunc dispatcher) {
    std::lock_guard lock(mutex_);
    dispatcher_ = std::move(dispatcher);
}

void UIDispatcher::Post(std::function<void()> callback) {
    if (!callback) return;
    DispatchFunc dispatcher_copy;
    {
        std::lock_guard lock(mutex_);
        dispatcher_copy = dispatcher_;
    }
    if (!dispatcher_copy) {
        NOVA_LOG_WARN("UIDispatcher not installed; dropping UI callback");
        return;
    }
    // 在锁外调用，避免持锁运行用户代码导致的重入/死锁
    dispatcher_copy(std::move(callback));
}

bool UIDispatcher::IsSet() {
    std::lock_guard lock(mutex_);
    return static_cast<bool>(dispatcher_);
}

}  // namespace nova::client
