#include "file_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

#include <chrono>
#include <ctime>

namespace nova {

namespace {
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
bool FileDaoImplT<DbMgr>::Insert(UserFile& file) {
    if (file.created_at.empty())
        file.created_at = NowUtcStr();
    file.updated_at = file.created_at;
    auto id = db_.DB().get_insert_id_after_insert(file);
    if (id == 0)
        return false;
    file.id = static_cast<int64_t>(id);
    return true;
}

template <typename DbMgr>
std::optional<UserFile> FileDaoImplT<DbMgr>::FindById(int64_t id) {
    auto res = db_.DB().template query_s<UserFile>("id=? AND status=1" /* Active */, id);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::optional<UserFile> FileDaoImplT<DbMgr>::FindLatestByUserAndType(int64_t user_id,
                                                                     const std::string& file_type) {
    // 使用 raw SQL 避免 ormpp WHERE-stripping bug（条件含 ORDER BY / LIMIT 时可能丢失 WHERE）
    // 使用 std::to_string(user_id) 安全拼接整数; file_type 需要防御性处理
    // 白名单校验：只允许已知的 file_type 值，防止 SQL 注入
    static constexpr std::string_view kAllowed[] = {"avatar", "image", "voice", "video", "file"};
    bool valid = false;
    for (auto sv : kAllowed) {
        if (file_type == sv) { valid = true; break; }
    }
    if (!valid)
        return std::nullopt;

    std::string sql = "SELECT * FROM user_files WHERE user_id = " + std::to_string(user_id) +
                      " AND file_type = '" + file_type + "' AND status = 1 ORDER BY id DESC LIMIT 1";
    auto res = db_.DB().template query_s<UserFile>(sql);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::vector<UserFile> FileDaoImplT<DbMgr>::ListByUserAndType(int64_t user_id, const std::string& file_type) {
    // 白名单校验：只允许已知的 file_type 值，防止 SQL 注入
    static constexpr std::string_view kAllowed[] = {"avatar", "image", "voice", "video", "file"};
    bool valid = false;
    for (auto sv : kAllowed) {
        if (file_type == sv) { valid = true; break; }
    }
    if (!valid)
        return {};

    std::string sql = "SELECT * FROM user_files WHERE user_id = " + std::to_string(user_id) +
                      " AND file_type = '" + file_type + "' AND status = 1 ORDER BY id DESC";
    return db_.DB().template query_s<UserFile>(sql);
}

template <typename DbMgr>
bool FileDaoImplT<DbMgr>::SoftDelete(int64_t id) {
    auto&& conn = db_.DB();
    auto res    = conn.template query_s<UserFile>("id=?", id);
    if (res.empty())
        return false;
    res[0].status     = static_cast<int>(FileStatus::Deleted);
    res[0].updated_at = NowUtcStr();
    return conn.template update_some<&UserFile::status, &UserFile::updated_at>(res[0]) == 1;
}

template <typename DbMgr>
bool FileDaoImplT<DbMgr>::UpdatePath(int64_t id, const std::string& path) {
    auto&& conn = db_.DB();
    auto res    = conn.template query_s<UserFile>("id=?", id);
    if (res.empty())
        return false;
    res[0].file_path = path;
    return conn.template update_some<&UserFile::file_path>(res[0]) == 1;
}

template <typename DbMgr>
bool FileDaoImplT<DbMgr>::SoftDeleteByUserAndType(int64_t user_id, const std::string& file_type) {
    auto&& conn = db_.DB();
    auto rows   = conn.template query_s<UserFile>("user_id=? AND file_type=? AND status=1" /* Active */, user_id, file_type);
    auto now    = NowUtcStr();
    bool all_ok = true;
    for (auto& f : rows) {
        f.status     = static_cast<int>(FileStatus::Deleted);
        f.updated_at = now;
        if (conn.template update_some<&UserFile::status, &UserFile::updated_at>(f) != 1) {
            all_ok = false;
        }
    }
    return all_ok;
}

// 显式实例化
template class FileDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class FileDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
