#include "ui_dispatcher.h"

namespace nova::client {

UIDispatcher::DispatchFunc UIDispatcher::dispatcher_;
std::mutex UIDispatcher::mutex_;

void UIDispatcher::Set(DispatchFunc dispatcher) {
    std::lock_guard lock(mutex_);
    dispatcher_ = std::move(dispatcher);
}

void UIDispatcher::Post(std::function<void()> callback) {
    std::lock_guard lock(mutex_);
    if (dispatcher_ && callback) {
        dispatcher_(std::move(callback));
    }
}

bool UIDispatcher::IsSet() {
    std::lock_guard lock(mutex_);
    return static_cast<bool>(dispatcher_);
}

}  // namespace nova::client
