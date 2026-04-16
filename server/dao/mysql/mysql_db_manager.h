#pragma once

#ifdef ORMPP_ENABLE_MYSQL

#include <dbng.hpp>
#include <mysql.hpp>
#include "../../model/types.h"
#include "../dao_factory.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace nova {

struct DatabaseConfig;

/// MySQL 数据库管理器：内置连接池 + 线程级连接固定
class MysqlDbManager {
public:
    MysqlDbManager() = default;
    ~MysqlDbManager();

    MysqlDbManager(const MysqlDbManager&)            = delete;
    MysqlDbManager& operator=(const MysqlDbManager&) = delete;

    bool Open(const DatabaseConfig& config);
    void Close();
    bool InitSchema();

    // ---- RAII 连接守卫 ----
    // 支持两种模式：
    //   owning  — 从连接池获取，析构时归还（默认）
    //   borrowed — 借用线程固定的连接，析构时不归还
    class ConnGuard {
    public:
        ~ConnGuard();
        ConnGuard(ConnGuard&& o) noexcept;
        ConnGuard(const ConnGuard&)            = delete;
        ConnGuard& operator=(const ConnGuard&) = delete;
        ConnGuard& operator=(ConnGuard&&)      = delete;

        explicit operator bool() const { return conn_ != nullptr; }

        template <typename T, typename... Args>
        auto query_s(const std::string& str, Args&&... args) {
            return conn_->query_s<T>(str, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        int insert(const T& t, Args&&... args) {
            return conn_->insert(t, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        uint64_t get_insert_id_after_insert(const T& t, Args&&... args) {
            return conn_->get_insert_id_after_insert(t, std::forward<Args>(args)...);
        }

        template <auto... members, typename T, typename... Args>
        int update_some(const T& t, Args&&... args) {
            return conn_->template update_some<members...>(t, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        uint64_t delete_records_s(const std::string& str, Args&&... args) {
            return conn_->template delete_records_s<T>(str, std::forward<Args>(args)...);
        }

        bool execute(const std::string& sql) { return conn_->execute(sql); }

        template <typename T, typename... Args>
        bool create_datatable(Args&&... args) {
            return conn_->template create_datatable<T>(std::forward<Args>(args)...);
        }

    private:
        friend class MysqlDbManager;
        // owning: 持有连接，析构归还池
        ConnGuard(MysqlDbManager* mgr, std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn);
        // borrowed: 借用原始指针，析构不归还
        explicit ConnGuard(ormpp::dbng<ormpp::mysql>* borrowed);

        MysqlDbManager* mgr_ = nullptr;
        std::unique_ptr<ormpp::dbng<ormpp::mysql>> owned_;
        ormpp::dbng<ormpp::mysql>* conn_ = nullptr;  // 始终指向有效连接
    };

    /// 获取连接：若当前线程已固定则返回 borrowed 守卫，否则从池中获取 owning 守卫
    ConnGuard DB();

    /// 创建 RAII 会话：固定一条连接到当前线程
    std::unique_ptr<DaoScopedConn> CreateSession();

private:
    friend class MysqlScopedConn;

    void ReturnConn(std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn);
    std::unique_ptr<ormpp::dbng<ormpp::mysql>> NewConn();
    std::unique_ptr<ormpp::dbng<ormpp::mysql>> AcquireFromPool();

    std::deque<std::unique_ptr<ormpp::dbng<ormpp::mysql>>> pool_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::string host_, user_, passwd_, dbname_;
    int port_      = 3306;
    int pool_size_ = 4;
    bool closed_   = false;
};

}  // namespace nova

#endif  // ORMPP_ENABLE_MYSQL
