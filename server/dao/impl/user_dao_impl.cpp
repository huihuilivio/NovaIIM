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
std::vector<User> UserDaoImplT<DbMgr>::FindByIds(const std::vector<int64_t>& ids) {
    if (ids.empty())
        return {};

    // 构建 IN 子句（int64_t 转字符串，无注入风险）
    std::string in_clause = "id IN (";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0)
            in_clause += ",";
        in_clause += std::to_string(ids[i]);
    }
    in_clause += ") AND status!=3";

    return db_.DB().template query_s<User>(in_clause);
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
std::optional<int64_t> UserDaoImplT<DbMgr>::UpdateStatus(const std::string& uid, int8_t status) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<User>("uid=? AND status!=3", uid);
    if (res.empty())
        return std::nullopt;
    res[0].status = status;
    if (conn.update_some<&User::status>(res[0]) != 1)
        return std::nullopt;
    return res[0].id;
}

template <typename DbMgr>
std::optional<int64_t> UserDaoImplT<DbMgr>::UpdatePassword(const std::string& uid, const std::string& password_hash) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<User>("uid=? AND status!=3", uid);
    if (res.empty())
        return std::nullopt;
    res[0].password_hash = password_hash;
    if (conn.update_some<&User::password_hash>(res[0]) != 1)
        return std::nullopt;
    return res[0].id;
}

template <typename DbMgr>
std::optional<int64_t> UserDaoImplT<DbMgr>::UpdateAvatar(const std::string& uid, const std::string& avatar) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<User>("uid=? AND status!=3", uid);
    if (res.empty())
        return std::nullopt;
    res[0].avatar = avatar;
    if (conn.update_some<&User::avatar>(res[0]) != 1)
        return std::nullopt;
    return res[0].id;
}

template <typename DbMgr>
std::optional<int64_t> UserDaoImplT<DbMgr>::UpdateNickname(const std::string& uid, const std::string& nickname) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<User>("uid=? AND status!=3", uid);
    if (res.empty())
        return std::nullopt;
    res[0].nickname = nickname;
    if (conn.update_some<&User::nickname>(res[0]) != 1)
        return std::nullopt;
    return res[0].id;
}

template <typename DbMgr>
std::optional<int64_t> UserDaoImplT<DbMgr>::SoftDelete(const std::string& uid) {
    return UpdateStatus(uid, static_cast<int8_t>(AccountStatus::Deleted));
}

template <typename DbMgr>
std::vector<User> UserDaoImplT<DbMgr>::SearchByNickname(const std::string& keyword, int limit) {
    // 限制搜索关键词长度，防止过大的 LIKE 查询
    if (keyword.size() > 128)
        return {};

    std::string escaped;
    escaped.reserve(keyword.size());
    for (char c : keyword) {
        if (c == '%' || c == '_' || c == '\\')
            escaped += '\\';
        escaped += c;
    }
    std::string like = "%" + escaped + "%";
    // 注意：ormpp 遇到条件中含 "limit" 会移除 WHERE，因此不使用 SQL LIMIT，而在代码中截断
    auto results = db_.DB().template query_s<User>("nickname LIKE ? ESCAPE '\\' AND status<>3", like);
    if (static_cast<int>(results.size()) > limit) {
        results.resize(static_cast<size_t>(limit));
    }
    return results;
}

template <typename DbMgr>
std::vector<UserDevice> UserDaoImplT<DbMgr>::ListDevicesByUser(const std::string& uid) {
    return db_.DB().query_s<UserDevice>("uid=?", uid);
}

template <typename DbMgr>
void UserDaoImplT<DbMgr>::UpsertDevice(const std::string& uid, const std::string& device_id,
                                       const std::string& device_type) {
    auto&& conn = db_.DB();
    auto res    = conn.query_s<UserDevice>("uid=? AND device_id=?", uid, device_id);
    if (res.empty()) {
        UserDevice dev;
        dev.uid            = uid;
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
