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

    // 构建 WHERE 子句
    std::string where = "status!=3";
    if (status >= 0) {
        where += " AND status=" + std::to_string(status);
    }

    if (keyword.empty()) {
        // 无关键词: 简单查询
        auto count_res = db_.DB().query_s<std::tuple<int64_t>>(
            "SELECT count(*) FROM users WHERE " + where);
        if (!count_res.empty()) {
            result.total = std::get<0>(count_res[0]);
        }

        auto items = db_.DB().query_s<User>(
            where + " ORDER BY id DESC LIMIT ? OFFSET ?", page_size, offset);
        result.items = std::move(items);
    } else {
        // 有关键词: 带 LIKE 查询，转义 % 和 _ 通配符
        std::string escaped;
        escaped.reserve(keyword.size());
        for (char c : keyword) {
            if (c == '%' || c == '_' || c == '\\') escaped += '\\';
            escaped += c;
        }
        std::string like = "%" + escaped + "%";
        std::string full_where = where + " AND (uid LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\')";

        auto count_res = db_.DB().query_s<std::tuple<int64_t>>(
            "SELECT count(*) FROM users WHERE " + full_where, like, like);
        if (!count_res.empty()) {
            result.total = std::get<0>(count_res[0]);
        }

        auto items = db_.DB().query_s<User>(
            full_where + " ORDER BY id DESC LIMIT ? OFFSET ?",
            like, like, page_size, offset);
        result.items = std::move(items);
    }

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
