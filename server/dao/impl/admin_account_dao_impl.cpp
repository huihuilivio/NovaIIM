#include "admin_account_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

template <typename DbMgr>
std::optional<Admin> AdminAccountDaoImplT<DbMgr>::FindByUid(const std::string& uid) {
    auto res = db_.DB().query_s<Admin>("uid=? AND status!=3" /* Deleted */, uid);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::optional<Admin> AdminAccountDaoImplT<DbMgr>::FindById(int64_t id) {
    auto res = db_.DB().query_s<Admin>("id=? AND status!=3" /* Deleted */, id);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
bool AdminAccountDaoImplT<DbMgr>::Insert(Admin& admin) {
    auto id = db_.DB().get_insert_id_after_insert(admin);
    if (id == 0)
        return false;
    admin.id = static_cast<int64_t>(id);
    return true;
}

template <typename DbMgr>
bool AdminAccountDaoImplT<DbMgr>::UpdatePassword(int64_t id, const std::string& password_hash) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<Admin>("id=? AND status!=3", id);
    if (res.empty())
        return false;
    res[0].password_hash = password_hash;
    return conn.template update_some<&Admin::password_hash>(res[0]) == 1;
}

template <typename DbMgr>
PaginatedAdmins AdminAccountDaoImplT<DbMgr>::ListAdmins(const std::string& keyword, int page, int page_size) {
    PaginatedAdmins result;
    auto&& conn = db_.DB();
    int64_t offset = static_cast<int64_t>(page - 1) * page_size;

    if (keyword.empty()) {
        auto count_rows = conn.query_s<std::tuple<int64_t>>(
            "SELECT COUNT(*) FROM admins WHERE status!=3");
        result.total = count_rows.empty() ? 0 : std::get<0>(count_rows[0]);

        result.items = conn.query_s<Admin>("status!=3 ORDER BY id DESC LIMIT ? OFFSET ?",
                                           page_size, offset);
    } else {
        // 转义 LIKE 通配符，防止注入
        std::string escaped;
        escaped.reserve(keyword.size());
        for (char c : keyword) {
            if (c == '%' || c == '_' || c == '\\')
                escaped += '\\';
            escaped += c;
        }
        std::string like = "%" + escaped + "%";
        auto count_rows = conn.query_s<std::tuple<int64_t>>(
            "SELECT COUNT(*) FROM admins WHERE status!=3 AND (uid LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\')",
            like, like);
        result.total = count_rows.empty() ? 0 : std::get<0>(count_rows[0]);

        result.items = conn.query_s<Admin>(
            "status!=3 AND (uid LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\') ORDER BY id DESC LIMIT ? OFFSET ?",
            like, like, page_size, offset);
    }
    return result;
}

template <typename DbMgr>
bool AdminAccountDaoImplT<DbMgr>::SoftDelete(int64_t id) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<Admin>("id=? AND status!=3", id);
    if (res.empty())
        return false;
    res[0].status = static_cast<int>(AccountStatus::Deleted);
    return conn.template update_some<&Admin::status>(res[0]) == 1;
}

template <typename DbMgr>
bool AdminAccountDaoImplT<DbMgr>::UpdateStatus(int64_t id, int status) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<Admin>("id=? AND status!=3", id);
    if (res.empty())
        return false;
    res[0].status = status;
    return conn.template update_some<&Admin::status>(res[0]) == 1;
}

// 显式实例化
template class AdminAccountDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class AdminAccountDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
