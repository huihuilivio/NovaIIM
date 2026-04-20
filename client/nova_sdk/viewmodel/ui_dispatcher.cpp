#include "ui_dispatcher.h"

namespace nova::client {

UIDispatcher::DispatchFunc UIDispatcher::dispatcher_;

void UIDispatcher::Set(DispatchFunc dispatcher) {
    dispatcher_ = std::move(dispatcher);
}

void UIDispatcher::Post(std::function<void()> callback) {
    if (dispatcher_ && callback) {
        dispatcher_(std::move(callback));
    }
}

bool UIDispatcher::IsSet() {
    return static_cast<bool>(dispatcher_);
}

}  // namespace nova::client
