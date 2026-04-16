#include "conn_manager.h"
#include <algorithm>

namespace nova {

void ConnManager::Add(int64_t user_id, ConnectionPtr conn) {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto& vec = shard.conns[user_id];

    bool was_empty = vec.empty();

    // 移除同一 device_id 的旧连接，避免重复推送
    auto did = conn->device_id();
    if (!did.empty()) {
        for (auto it = vec.begin(); it != vec.end(); ) {
            if ((*it)->device_id() == did) {
                (*it)->Close();
                it = vec.erase(it);
            } else {
                ++it;
            }
        }
    }
    vec.push_back(std::move(conn));

    if (was_empty) {
        online_count_.fetch_add(1, std::memory_order_relaxed);
    }
}

void ConnManager::Remove(int64_t user_id, Connection* conn) {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.conns.find(user_id);
    if (it == shard.conns.end()) return;

    auto& vec = it->second;
    vec.erase(
        std::remove_if(vec.begin(), vec.end(),
            [conn](const ConnectionPtr& c) { return c.get() == conn; }),
        vec.end()
    );

    if (vec.empty()) {
        shard.conns.erase(it);
        online_count_.fetch_sub(1, std::memory_order_relaxed);
    }
}

std::vector<ConnectionPtr> ConnManager::GetConns(int64_t user_id) const {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.conns.find(user_id);
    if (it == shard.conns.end()) return {};
    return it->second;
}

ConnectionPtr ConnManager::GetConn(int64_t user_id, const std::string& device_id) const {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.conns.find(user_id);
    if (it == shard.conns.end()) return nullptr;

    for (const auto& c : it->second) {
        if (c->device_id() == device_id) return c;
    }
    return nullptr;
}

bool ConnManager::IsOnline(int64_t user_id) const {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.conns.find(user_id);
    return it != shard.conns.end() && !it->second.empty();
}

} // namespace nova
