#include "conn_manager.h"
#include <algorithm>

namespace nova {

void ConnManager::Add(int64_t user_id, ConnectionPtr conn) {
    static constexpr size_t kMaxConnsPerUser = 10;
    std::vector<ConnectionPtr> to_close;  // 延迟关闭，避免锁内 Close()
    {
        auto& shard = GetShard(user_id);
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto& vec = shard.conns[user_id];

        // 在任何移除操作之前记录用户是否处于离线状态，
        // 用于判断是否需要增加 online_count。
        // 若用户已在线（vec 非空），后续的设备淘汰不改变在线状态。
        bool was_offline = vec.empty();

        // 移除同一 device_id 的旧连接，避免重复推送
        auto did = conn->device_id();
        if (!did.empty()) {
            for (auto it = vec.begin(); it != vec.end();) {
                if ((*it)->device_id() == did) {
                    to_close.push_back(std::move(*it));
                    it = vec.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // 连接数上限：驱逐最旧的连接防止资源耗尽
        while (vec.size() >= kMaxConnsPerUser) {
            to_close.push_back(std::move(vec.front()));
            vec.erase(vec.begin());
        }

        vec.push_back(std::move(conn));

        if (was_offline) {
            online_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    // 锁已释放，安全关闭旧连接
    for (auto& c : to_close) {
        c->Close();
    }
}

void ConnManager::Remove(int64_t user_id, Connection* conn) {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.conns.find(user_id);
    if (it == shard.conns.end())
        return;

    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(), [conn](const ConnectionPtr& c) { return c.get() == conn; }),
              vec.end());

    if (vec.empty()) {
        shard.conns.erase(it);
        online_count_.fetch_sub(1, std::memory_order_relaxed);
    }
}

std::vector<ConnectionPtr> ConnManager::GetConns(int64_t user_id) const {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.conns.find(user_id);
    if (it == shard.conns.end())
        return {};
    return it->second;
}

ConnectionPtr ConnManager::GetConn(int64_t user_id, const std::string& device_id) const {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.conns.find(user_id);
    if (it == shard.conns.end())
        return nullptr;

    for (const auto& c : it->second) {
        if (c->device_id() == device_id)
            return c;
    }
    return nullptr;
}

bool ConnManager::IsOnline(int64_t user_id) const {
    auto& shard = GetShard(user_id);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.conns.find(user_id);
    return it != shard.conns.end() && !it->second.empty();
}

}  // namespace nova
