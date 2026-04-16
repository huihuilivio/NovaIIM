#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nova {

// 滑动窗口频率限制器
// 用于登录防暴力破解：追踪每个 key（uid/IP）在时间窗口内的失败次数
class RateLimiter {
public:
    RateLimiter(int max_attempts, std::chrono::seconds window)
        : max_attempts_(max_attempts), window_(window) {}

    // 检查是否允许（未超限）。不计数。
    bool Allow(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto it = entries_.find(key);
        if (it == entries_.end()) return true;
        auto& e = it->second;
        if (now - e.window_start >= window_) {
            entries_.erase(it);
            return true;
        }
        return e.count < max_attempts_;
    }

    // 记录一次失败尝试
    void RecordFailure(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto& e = entries_[key];
        if (e.count == 0 || now - e.window_start >= window_) {
            e.window_start = now;
            e.count = 1;
        } else {
            ++e.count;
        }
    }

    // 成功后重置（如登录成功）
    void Reset(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.erase(key);
    }

private:
    struct Entry {
        std::chrono::steady_clock::time_point window_start{};
        int count = 0;
    };

    int max_attempts_;
    std::chrono::seconds window_;
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace nova
