#include "user_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

#include <chrono>
#include <ctime>

namespace nova {

namespace {
// ISO-8601 UTC 时间戳
inline std::string NowUtcStr() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    struct tm tm_buf {};
#ifdef _MSC_VER
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}
}  // namespace

template <typename DbMgr>
std::optional<User> UserDaoImplT<DbMgr>::FindByUid(const std::string& uid) {
    auto res = db_.DB().query_s<User>("uid=? AND status!=3" /* Deleted */, uid);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::optional<User> UserDaoImplT<DbMgr>::FindByEmail(const std::string& email) {
    auto res = db_.DB().query_s<User>("email=? AND status!=3" /* Deleted */, email);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::optional<User> UserDaoImplT<DbMgr>::FindById(int64_t id) {
    auto res = db_.DB().query_s<User>("id=? AND status!=3" /* Deleted */, id);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
UserListResult UserDaoImplT<DbMgr>::ListUsers(const std::string& keyword, int status, int page, int page_size) {
    UserListResult result;
    int offset = (page - 1) * page_size;

    // 使用参数化恒等式避免字符串拼接
    // status < 0 时 (? < 0) 恒真，跳过过滤；否则精确匹配 status
    // keyword 为空时 (? = '') 恒真，跳过 LIKE 过滤
    std::string escaped;
    if (!keyword.empty()) {
        escaped.reserve(keyword.size());
        for (char c : keyword) {
            if (c == '%' || c == '_' || c == '\\')
                escaped += '\\';
            escaped += c;
        }
    }
    std::string like = keyword.empty() ? "" : "%" + escaped + "%";

    static constexpr auto kCountSql =
        "SELECT count(*) FROM users WHERE status!=3 "
        "AND (? < 0 OR status = ?) "
        "AND (? = '' OR uid LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\' OR email LIKE ? ESCAPE '\\')";

    static constexpr auto kWhere =
        "status!=3 "
        "AND (? < 0 OR status = ?) "
        "AND (? = '' OR uid LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\' OR email LIKE ? ESCAPE '\\') "
        "ORDER BY id DESC LIMIT ? OFFSET ?";

    auto&& conn = db_.DB();

    auto count_res = conn.query_s<std::tuple<int64_t>>(kCountSql, status, status, like, like, like, like);
    if (!count_res.empty()) {
        result.total = std::get<0>(count_res[0]);
    }

    result.items = conn.query_s<User>(kWhere, status, status, like, like, like, like, page_size, offset);

    return result;
}

template <typename DbMgr>
bool UserDaoImplT<DbMgr>::Insert(User& user) {
    auto id = db_.DB().get_insert_id_after_insert(user);
    if (id == 0)
        return false;
    user.id = static_cast<int64_t>(id);
    return true;
}

template <typename DbMgr>
bool UserDaoImplT<DbMgr>::UpdateStatus(int64_t id, int8_t status) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<User>("id=?", id);
    if (res.empty())
        return false;
    res[0].status = status;
    return conn.update_some<&User::status>(res[0]) == 1;
}

template <typename DbMgr>
bool UserDaoImplT<DbMgr>::UpdatePassword(int64_t id, const std::string& password_hash) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<User>("id=?", id);
    if (res.empty())
        return false;
    res[0].password_hash = password_hash;
    return conn.update_some<&User::password_hash>(res[0]) == 1;
}

template <typename DbMgr>
bool UserDaoImplT<DbMgr>::UpdateAvatar(int64_t id, const std::string& avatar) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<User>("id=?", id);
    if (res.empty())
        return false;
    res[0].avatar = avatar;
    return conn.update_some<&User::avatar>(res[0]) == 1;
}

template <typename DbMgr>
bool UserDaoImplT<DbMgr>::SoftDelete(int64_t id) {
    return UpdateStatus(id, static_cast<int>(AccountStatus::Deleted));
}

template <typename DbMgr>
std::vector<UserDevice> UserDaoImplT<DbMgr>::ListDevicesByUser(int64_t user_id) {
    return db_.DB().query_s<UserDevice>("user_id=?", user_id);
}

template <typename DbMgr>
void UserDaoImplT<DbMgr>::UpsertDevice(int64_t user_id, const std::string& device_id,
                                       const std::string& device_type) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<UserDevice>("user_id=? AND device_id=?", user_id, device_id);
    if (res.empty()) {
        UserDevice dev;
        dev.user_id        = user_id;
        dev.device_id      = device_id;
        dev.device_type    = device_type;
        dev.last_active_at = NowUtcStr();
        conn.insert(dev);
    } else {
        res[0].device_type    = device_type;
        res[0].last_active_at = NowUtcStr();
        conn.update_some<&UserDevice::device_type, &UserDevice::last_active_at>(res[0]);
    }
}

// 显式实例化
template class UserDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class UserDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
