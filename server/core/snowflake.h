#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>

namespace nova {

/// Snowflake ID 生成器（Twitter Snowflake 变体）
///
/// 64-bit 布局（最高位保留为 0，保证 int64_t 正数）:
///   0 | timestamp (41 bits) | node_id (10 bits) | sequence (12 bits)
///
/// - timestamp: 毫秒级，自定义纪元起（2024-01-01 00:00:00 UTC）
///   41 bits ≈ 69.7 年（至 2093 年）
/// - node_id: 0–1023，支持最多 1024 个节点
/// - sequence: 0–4095，同一毫秒内最多生成 4096 个 ID
///
/// 线程安全（内部自旋锁保护）
class Snowflake {
public:
    static constexpr int kTimestampBits = 41;
    static constexpr int kNodeBits      = 10;
    static constexpr int kSequenceBits  = 12;

    static constexpr int64_t kMaxNodeId   = (1LL << kNodeBits) - 1;      // 1023
    static constexpr int64_t kMaxSequence = (1LL << kSequenceBits) - 1;   // 4095

    // 自定义纪元：2024-01-01 00:00:00 UTC（毫秒）
    static constexpr int64_t kEpoch = 1704067200000LL;

    explicit Snowflake(int node_id = 0) : node_id_(node_id) {
        if (node_id < 0 || node_id > kMaxNodeId) {
            throw std::invalid_argument("Snowflake node_id must be in [0, " +
                                        std::to_string(kMaxNodeId) + "], got " +
                                        std::to_string(node_id));
        }
    }

    /// 生成下一个唯一 ID（int64_t，始终 > 0）
    int64_t NextId() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = CurrentMillis();

        if (now < last_ms_) {
            // Clock moved backward — wait until it catches up to avoid duplicate IDs.
            // This can happen during NTP step corrections.
            now = WaitNextMillis(last_ms_);
        }

        if (now == last_ms_) {
            sequence_ = (sequence_ + 1) & kMaxSequence;
            if (sequence_ == 0) {
                // 同一毫秒内序列号耗尽，等待下一毫秒
                now = WaitNextMillis(now);
            }
        } else {
            sequence_ = 0;
        }

        last_ms_ = now;

        return ((now - kEpoch) << (kNodeBits + kSequenceBits)) |
               (static_cast<int64_t>(node_id_) << kSequenceBits) |
               sequence_;
    }

    /// 生成下一个唯一 ID 的字符串形式
    std::string NextIdStr() {
        return std::to_string(NextId());
    }

    int node_id() const { return node_id_; }

private:
    static int64_t CurrentMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    int64_t WaitNextMillis(int64_t current) {
        int64_t now = CurrentMillis();
        while (now <= current) {
            now = CurrentMillis();
        }
        return now;
    }

    int node_id_;
    int64_t last_ms_  = 0;
    int64_t sequence_ = 0;
    std::mutex mutex_;
};

}  // namespace nova
