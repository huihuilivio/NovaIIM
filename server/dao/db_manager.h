#pragma once

#include <string>

#include <dbng.hpp>
#include <sqlite.hpp>

#include "../model/types.h"

namespace nova {

// ormpp SQLite3 数据库管理器
// 整个进程只需一个实例，main.cpp 启动时 Open，关闭时 Close
class DbManager {
public:
    DbManager() = default;
    ~DbManager() = default;

    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    // 打开数据库
    bool Open(const std::string& path);

    // 关闭数据库
    void Close();

    // 建表（幂等）
    bool InitSchema();

    // 获取底层 ormpp 数据库对象
    ormpp::dbng<ormpp::sqlite>& DB() { return db_; }

private:
    ormpp::dbng<ormpp::sqlite> db_;
};

} // namespace nova
