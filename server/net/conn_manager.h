#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "connection.h"

namespace nova {

// 多端连接管理（对应架构文档 4.2 ConnManager）
// unordered_map<user_id, vector<Connection*>>
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

private:
    mutable std::mutex mutex_;
    std::unordered_map<int64_t, std::vector<ConnectionPtr>> conns_;
};

} // namespace nova
