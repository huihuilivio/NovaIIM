#pragma once
// Observable<T> — 可观察属性，用于数据驱动 UI
//
// ViewModel 通过 Observable 暴露状态，View 通过 Observe() 订阅变更
// 支持多个观察者，设置值时自动通知
//
// 用法:
//   Observable<int> count;
//   count.Observe([](const int& v) { printf("count = %d\n", v); });
//   count.Set(42);  // 触发通知

#include <export.h>

#include <functional>
#include <mutex>
#include <vector>

namespace nova::client {

template <typename T>
class Observable {
public:
    using Observer = std::function<void(const T&)>;

    Observable() = default;
    explicit Observable(T value) : value_(std::move(value)) {}

    /// 获取当前值
    const T& Get() const { return value_; }

    /// 设置新值并通知所有观察者
    void Set(T value) {
        std::vector<Observer> snapshot;
        {
            std::lock_guard lock(mutex_);
            value_ = std::move(value);
            snapshot = observers_;
        }
        for (auto& cb : snapshot) cb(value_);
    }

    /// 订阅变更通知（立即以当前值触发一次回调）
    void Observe(Observer cb) {
        T current;
        {
            std::lock_guard lock(mutex_);
            observers_.push_back(cb);
            current = value_;
        }
        cb(current);
    }

    /// 清除所有观察者
    void ClearObservers() {
        std::lock_guard lock(mutex_);
        observers_.clear();
    }

private:
    T value_{};
    mutable std::mutex mutex_;
    std::vector<Observer> observers_;
};

}  // namespace nova::client
