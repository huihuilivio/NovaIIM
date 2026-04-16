#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "connection.h"

namespace nova {

// 多端连接管理（对应架构文档 4.2 ConnManager）
// unordered_map<user_id, vector<Connection*>>
// 通过 ServerContext::conn_manager() 访问，不再建议直接使用 Instance()
class ConnManager {
public:
    ConnManager() = default;

    // 保留全局单例用于向后兼容，新代码应通过 ServerContext 注入
    static ConnManager& Instance() {
        static ConnManager inst;
        return inst;
    }

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
