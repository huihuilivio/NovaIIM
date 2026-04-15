#pragma once

#ifdef ORMPP_ENABLE_MYSQL

#include <dbng.hpp>
#include <mysql.hpp>
#include "../../model/types.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace nova {

struct DatabaseConfig;

/// MySQL 数据库管理器：内置连接池
class MysqlDbManager {
public:
    MysqlDbManager() = default;
    ~MysqlDbManager();

    MysqlDbManager(const MysqlDbManager&) = delete;
    MysqlDbManager& operator=(const MysqlDbManager&) = delete;

    bool Open(const DatabaseConfig& config);
    void Close();
    bool InitSchema();

    // ---- RAII 连接守卫 ----
    class ConnGuard {
    public:
        ~ConnGuard();
        ConnGuard(ConnGuard&& o) noexcept;
        ConnGuard(const ConnGuard&) = delete;
        ConnGuard& operator=(const ConnGuard&) = delete;
        ConnGuard& operator=(ConnGuard&&) = delete;

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

        bool execute(const std::string& sql) {
            return conn_->execute(sql);
        }

        template <typename T, typename... Args>
        bool create_datatable(Args&&... args) {
            return conn_->template create_datatable<T>(std::forward<Args>(args)...);
        }

    private:
        friend class MysqlDbManager;
        ConnGuard(MysqlDbManager* mgr, std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn);

        MysqlDbManager* mgr_ = nullptr;
        std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn_;
    };

    ConnGuard DB();

private:
    void ReturnConn(std::unique_ptr<ormpp::dbng<ormpp::mysql>> conn);
    std::unique_ptr<ormpp::dbng<ormpp::mysql>> NewConn();

    std::deque<std::unique_ptr<ormpp::dbng<ormpp::mysql>>> pool_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::string host_, user_, passwd_, dbname_;
    int port_ = 3306;
    int pool_size_ = 4;
    bool closed_ = false;
};

}  // namespace nova

#endif  // ORMPP_ENABLE_MYSQL
