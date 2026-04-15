#include "user_dao_impl.h"

namespace nova {

std::optional<User> UserDaoImpl::FindByUid(const std::string& uid) {
    auto res = db_.DB().query_s<User>("uid=? AND status!=3", uid);
    if (res.empty()) return std::nullopt;
    return res[0];
}

std::optional<User> UserDaoImpl::FindById(int64_t id) {
    auto res = db_.DB().query_s<User>("id=? AND status!=3", id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

UserListResult UserDaoImpl::ListUsers(const std::string& keyword, int status,
                                       int page, int page_size) {
    UserListResult result;
    int offset = (page - 1) * page_size;

    // 使用参数化恒等式避免字符串拼接
    // status < 0 时 (? < 0) 恒真，跳过过滤；否则精确匹配 status
    // keyword 为空时 (? = '') 恒真，跳过 LIKE 过滤
    std::string escaped;
    if (!keyword.empty()) {
        escaped.reserve(keyword.size());
        for (char c : keyword) {
            if (c == '%' || c == '_' || c == '\\') escaped += '\\';
            escaped += c;
        }
    }
    std::string like = keyword.empty() ? "" : "%" + escaped + "%";

    static constexpr auto kCountSql =
        "SELECT count(*) FROM users WHERE status!=3 "
        "AND (? < 0 OR status = ?) "
        "AND (? = '' OR uid LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\')";

    static constexpr auto kWhere =
        "status!=3 "
        "AND (? < 0 OR status = ?) "
        "AND (? = '' OR uid LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\') "
        "ORDER BY id DESC LIMIT ? OFFSET ?";

    auto count_res = db_.DB().query_s<std::tuple<int64_t>>(
        kCountSql, status, status, like, like, like);
    if (!count_res.empty()) {
        result.total = std::get<0>(count_res[0]);
    }

    result.items = db_.DB().query_s<User>(
        kWhere, status, status, like, like, like, page_size, offset);

    return result;
}

bool UserDaoImpl::Insert(User& user) {
    auto id = db_.DB().get_insert_id_after_insert(user);
    if (id == 0) return false;
    user.id = static_cast<int64_t>(id);
    return true;
}

bool UserDaoImpl::UpdateStatus(int64_t id, int8_t status) {
    // 查出完整对象，修改字段，通过 update_some 安全更新（内部用 prepared statement）
    auto res = db_.DB().query_s<User>("id=?", id);
    if (res.empty()) return false;
    res[0].status = status;
    return db_.DB().update_some<&User::status>(res[0]) == 1;
}

bool UserDaoImpl::UpdatePassword(int64_t id, const std::string& password_hash) {
    auto res = db_.DB().query_s<User>("id=?", id);
    if (res.empty()) return false;
    res[0].password_hash = password_hash;
    return db_.DB().update_some<&User::password_hash>(res[0]) == 1;
}

bool UserDaoImpl::SoftDelete(int64_t id) {
    return UpdateStatus(id, 3);
}

} // namespace nova
