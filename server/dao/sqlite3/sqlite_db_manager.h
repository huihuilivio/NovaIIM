#pragma once

#include <string>

#include <dbng.hpp>
#include <sqlite.hpp>

#include "../../model/types.h"

// ormpp 需要显式注册 auto_key 才能在 insert 时跳过 id 列
namespace nova::detail {
inline const int kAutoKeysRegistered = [] {
    ormpp::add_auto_key_field("nova::User", "id");
    ormpp::add_auto_key_field("nova::Admin", "id");
    ormpp::add_auto_key_field("nova::UserDevice", "id");
    ormpp::add_auto_key_field("nova::Message", "id");
    ormpp::add_auto_key_field("nova::Conversation", "id");
    ormpp::add_auto_key_field("nova::ConversationMember", "id");
    ormpp::add_auto_key_field("nova::AuditLog", "id");
    ormpp::add_auto_key_field("nova::AdminSession", "id");
    ormpp::add_auto_key_field("nova::Role", "id");
    ormpp::add_auto_key_field("nova::Permission", "id");
    ormpp::add_auto_key_field("nova::RolePermission", "id");
    ormpp::add_auto_key_field("nova::AdminRole", "id");
    ormpp::add_auto_key_field("nova::UserFile", "id");
    return 0;
}();
}  // namespace nova::detail

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

private:
    ormpp::dbng<ormpp::sqlite> db_;
};

}  // namespace nova
