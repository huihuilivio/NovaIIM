#pragma once
// EventBus — 发布-订阅事件总线（线程安全）
//
// 用法：
//   EventBus::Get().Subscribe<LoginAck>([](const LoginAck& ack) { ... });
//   EventBus::Get().Publish(login_ack);

#include <core/export.h>

#include <any>
#include <functional>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace nova::client {

class NOVA_CLIENT_API EventBus {
public:
    static EventBus& Get();

    /// 订阅事件（线程安全）
    template <typename T>
    void Subscribe(std::function<void(const T&)> handler) {
        std::lock_guard lock(mutex_);
        auto key = std::type_index(typeid(T));
        handlers_[key].emplace_back([h = std::move(handler)](const std::any& evt) {
            h(std::any_cast<const T&>(evt));
        });
    }

    /// 发布事件（同步调用所有订阅者）
    template <typename T>
    void Publish(const T& event) {
        std::vector<std::function<void(const std::any&)>> snapshot;
        {
            std::lock_guard lock(mutex_);
            auto it = handlers_.find(std::type_index(typeid(T)));
            if (it == handlers_.end()) return;
            snapshot = it->second;
        }
        std::any wrapped = event;
        for (auto& h : snapshot) {
            h(wrapped);
        }
    }

    /// 清除所有订阅
    void Clear();

    /// 清除指定类型的订阅
    template <typename T>
    void Unsubscribe() {
        std::lock_guard lock(mutex_);
        handlers_.erase(std::type_index(typeid(T)));
    }

private:
    EventBus() = default;

    std::mutex mutex_;
    std::unordered_map<std::type_index,
                       std::vector<std::function<void(const std::any&)>>>
        handlers_;
};

}  // namespace nova::client
