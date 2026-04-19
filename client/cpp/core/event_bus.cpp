#include "event_bus.h"

namespace nova::client {

EventBus& EventBus::Get() {
    static EventBus instance;
    return instance;
}

void EventBus::Clear() {
    std::lock_guard lock(mutex_);
    handlers_.clear();
}

}  // namespace nova::client
