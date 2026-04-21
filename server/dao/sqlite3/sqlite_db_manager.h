#pragma once

#include <mutex>
#include <string>

#include <dbng.hpp>
#include <sqlite.hpp>

#include "../../model/types.h"

namespace nova {

// ormpp SQLite3 数据库管理器
class SqliteDbManager {
public:
    SqliteDbManager()  = default;
    ~SqliteDbManager() = default;

    SqliteDbManager(const SqliteDbManager&)            = delete;
    SqliteDbManager& operator=(const SqliteDbManager&) = delete;

    bool Open(const std::string& path);
    void Close();
    bool InitSchema();

    ormpp::dbng<ormpp::sqlite>& DB() { return db_; }

    /// 全局互斥锁：SQLite 单连接，多线程需串行访问
    std::recursive_mutex& Mutex() { return mu_; }

private:
    ormpp::dbng<ormpp::sqlite> db_;
    std::recursive_mutex       mu_;
};

}  // namespace nova
