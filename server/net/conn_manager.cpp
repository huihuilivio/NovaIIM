#include "conn_manager.h"
#include <algorithm>

namespace nova {

void ConnManager::Add(int64_t user_id, ConnectionPtr conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& vec = conns_[user_id];
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
}

void ConnManager::Remove(int64_t user_id, Connection* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = conns_.find(user_id);
    if (it == conns_.end()) return;

    auto& vec = it->second;
    vec.erase(
        std::remove_if(vec.begin(), vec.end(),
            [conn](const ConnectionPtr& c) { return c.get() == conn; }),
        vec.end()
    );

    if (vec.empty()) {
        conns_.erase(it);
    }
}

std::vector<ConnectionPtr> ConnManager::GetConns(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = conns_.find(user_id);
    if (it == conns_.end()) return {};
    return it->second;
}

ConnectionPtr ConnManager::GetConn(int64_t user_id, const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = conns_.find(user_id);
    if (it == conns_.end()) return nullptr;

    for (const auto& c : it->second) {
        if (c->device_id() == device_id) return c;
    }
    return nullptr;
}

bool ConnManager::IsOnline(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = conns_.find(user_id);
    return it != conns_.end() && !it->second.empty();
}

} // namespace nova
