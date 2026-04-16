#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include "app_config.h"
#include "../dao/dao_factory.h"
#include "../net/conn_manager.h"

namespace nova {

// 服务上下文 —— 线程安全的运行时状态中心
// 各模块通过接口读写共享指标，不持有对方指针/引用
// 所有计数器使用 atomic，可从任意线程安全读写
class ServerContext {
public:
    explicit ServerContext(const AppConfig& cfg)
        : config_(cfg)
        , start_time_(std::chrono::steady_clock::now()) {}

    // --- 配置（只读）---
    const AppConfig& config() const { return config_; }

    // --- DAO 工厂（全局唯一）---
    void set_dao(std::unique_ptr<DaoFactory> dao) { dao_ = std::move(dao); }
    DaoFactory& dao() const { return *dao_; }

    // --- 连接管理器（显式依赖注入，避免各模块直接使用单例）---
    ConnManager& conn_manager() { return conn_manager_; }
    const ConnManager& conn_manager() const { return conn_manager_; }

    // --- 连接指标 ---
    int  connection_count() const { return conn_count_.load(std::memory_order_relaxed); }
    void add_connection()         { conn_count_.fetch_add(1, std::memory_order_relaxed); }
    void remove_connection()      { conn_count_.fetch_sub(1, std::memory_order_relaxed); }

    // --- 在线用户数（已认证的连接数）---
    int  online_user_count() const { return online_users_.load(std::memory_order_relaxed); }
    void add_online_user()         { online_users_.fetch_add(1, std::memory_order_relaxed); }
    void remove_online_user()      { online_users_.fetch_sub(1, std::memory_order_relaxed); }

    // --- 消息统计 ---
    int64_t total_messages_in() const  { return msgs_in_.load(std::memory_order_relaxed); }
    void    incr_messages_in()         { msgs_in_.fetch_add(1, std::memory_order_relaxed); }

    int64_t total_messages_out() const { return msgs_out_.load(std::memory_order_relaxed); }
    void    incr_messages_out()        { msgs_out_.fetch_add(1, std::memory_order_relaxed); }

    // --- 无效包计数 ---
    int64_t bad_packets() const { return bad_packets_.load(std::memory_order_relaxed); }
    void    incr_bad_packets()  { bad_packets_.fetch_add(1, std::memory_order_relaxed); }

    // --- 运行时长 ---
    int64_t uptime_seconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    }

private:
    AppConfig config_;
    std::unique_ptr<DaoFactory> dao_;
    ConnManager conn_manager_;
    std::chrono::steady_clock::time_point start_time_;

    std::atomic<int>     conn_count_{0};
    std::atomic<int>     online_users_{0};
    std::atomic<int64_t> msgs_in_{0};
    std::atomic<int64_t> msgs_out_{0};
    std::atomic<int64_t> bad_packets_{0};
};

} // namespace nova
