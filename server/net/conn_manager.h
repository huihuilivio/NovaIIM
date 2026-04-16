#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "connection.h"

namespace nova {

// 多端连接管理（对应架构文档 4.2 ConnManager）
// 使用分片锁（sharded lock）降低高并发下的锁争用
// 通过 ServerContext::conn_manager() 访问
class ConnManager {
public:
    ConnManager() = default;

    // 禁止拷贝（内含 mutex）
    ConnManager(const ConnManager&) = delete;
    ConnManager& operator=(const ConnManager&) = delete;

    // 注册连接
    void Add(int64_t user_id, ConnectionPtr conn);

    // 移除连接
    void Remove(int64_t user_id, Connection* conn);

    // 获取用户的所有连接（多端）
    std::vector<ConnectionPtr> GetConns(int64_t user_id) const;

    // 获取用户的特定设备连接
    ConnectionPtr GetConn(int64_t user_id, const std::string& device_id) const;

    // 用户是否在线
    bool IsOnline(int64_t user_id) const;

    // 在线用户数（由 Add/Remove 自动维护，替代手动计数）
    int online_count() const { return online_count_.load(std::memory_order_relaxed); }

private:
    static constexpr size_t kShardCount = 16;

    struct Shard {
        mutable std::mutex mutex;
        std::unordered_map<int64_t, std::vector<ConnectionPtr>> conns;
    };

    Shard&       GetShard(int64_t user_id)       { return shards_[static_cast<size_t>(user_id) % kShardCount]; }
    const Shard& GetShard(int64_t user_id) const { return shards_[static_cast<size_t>(user_id) % kShardCount]; }

    std::array<Shard, kShardCount> shards_;
    std::atomic<int> online_count_{0};
};

} // namespace nova
