#pragma once
// CacheDatabase — SDK 本地 SQLite 缓存数据库
//
// 管理数据库连接、schema 初始化、PRAGMA 配置。
// 每个登录用户独立数据库文件：{data_dir}/{uid}/cache.db

#include <cache/cache_models.h>

#include <dbng.hpp>
#include <sqlite.hpp>

#include <string>

namespace nova::client {

class CacheDatabase {
public:
    CacheDatabase()  = default;
    ~CacheDatabase();

    CacheDatabase(const CacheDatabase&)            = delete;
    CacheDatabase& operator=(const CacheDatabase&) = delete;

    /// 打开数据库（自动创建目录和 schema）
    bool Open(const std::string& data_dir, const std::string& uid);

    /// 关闭数据库
    void Close();

    /// 是否已打开
    bool IsOpen() const { return open_; }

    /// 获取 ormpp 数据库连接（DAO 层使用）
    ormpp::dbng<ormpp::sqlite>& DB() { return db_; }

private:
    bool InitSchema();

    ormpp::dbng<ormpp::sqlite> db_;
    bool open_ = false;
};

}  // namespace nova::client
