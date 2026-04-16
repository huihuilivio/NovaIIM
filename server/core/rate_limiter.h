#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nova {

// 滑动窗口频率限制器
// 用于登录防暴力破解：追踪每个 key（uid/IP）在时间窗口内的失败次数
// 内置过期清理：每 kPurgeInterval 次写操作触发全量扫描，防止内存无限增长
class RateLimiter {
public:
    RateLimiter(int max_attempts, std::chrono::seconds window, size_t max_entries = 100000)
        : max_attempts_(max_attempts), window_(window), max_entries_(max_entries) {}

    // 检查是否允许（未超限）。不计数。
    bool Allow(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto it  = entries_.find(key);
        if (it == entries_.end())
            return true;
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

        MaybePurge(now);

        auto& e = entries_[key];
        if (e.count == 0 || now - e.window_start >= window_) {
            e.window_start = now;
            e.count        = 1;
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

    // 清理过期条目（调用方需已持锁）
    // 触发条件：写计数达到 kPurgeInterval 或条目数超限
    void MaybePurge(std::chrono::steady_clock::time_point now) {
        ++write_count_;
        if (write_count_ < kPurgeInterval && entries_.size() < max_entries_) {
            return;
        }
        write_count_ = 0;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (now - it->second.window_start >= window_) {
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
    }

    static constexpr size_t kPurgeInterval = 1024;

    int max_attempts_;
    std::chrono::seconds window_;
    size_t max_entries_;
    size_t write_count_ = 0;
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace nova
