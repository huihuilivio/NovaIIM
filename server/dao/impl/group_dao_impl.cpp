#include "group_dao_impl.h"
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

// ---- Group ----

template <typename DbMgr>
bool GroupDaoImplT<DbMgr>::InsertGroup(Group& group) {
    if (group.created_at.empty())
        group.created_at = NowUtcStr();
    auto id = db_.DB().get_insert_id_after_insert(group);
    if (id == 0)
        return false;
    group.id = static_cast<int64_t>(id);
    return true;
}

template <typename DbMgr>
std::optional<Group> GroupDaoImplT<DbMgr>::FindByConversationId(int64_t conversation_id) {
    auto res = db_.DB().template query_s<Group>("conversation_id=?", conversation_id);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
bool GroupDaoImplT<DbMgr>::UpdateGroup(const Group& group) {
    return db_.DB().update(group) == 1;
}

template <typename DbMgr>
bool GroupDaoImplT<DbMgr>::DeleteByConversationId(int64_t conversation_id) {
    std::string sql = "DELETE FROM groups WHERE conversation_id = " +
                      std::to_string(conversation_id);
    return db_.DB().execute(sql);
}

template <typename DbMgr>
std::vector<Group> GroupDaoImplT<DbMgr>::FindGroupsByUser(int64_t user_id) {
    std::string sql =
        "SELECT g.* FROM groups g "
        "INNER JOIN conversation_members cm ON cm.conversation_id = g.conversation_id "
        "WHERE cm.user_id = " + std::to_string(user_id);
    return db_.DB().template query_s<Group>(sql);
}

// ---- GroupJoinRequest ----

template <typename DbMgr>
bool GroupDaoImplT<DbMgr>::InsertJoinRequest(GroupJoinRequest& req) {
    if (req.created_at.empty())
        req.created_at = NowUtcStr();
    req.updated_at = req.created_at;
    auto id = db_.DB().get_insert_id_after_insert(req);
    if (id == 0)
        return false;
    req.id = static_cast<int64_t>(id);
    return true;
}

template <typename DbMgr>
std::optional<GroupJoinRequest> GroupDaoImplT<DbMgr>::FindPendingJoinRequest(
    int64_t conversation_id, int64_t user_id) {
    auto res = db_.DB().template query_s<GroupJoinRequest>(
        "conversation_id=? AND user_id=? AND status=0", conversation_id, user_id);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::optional<GroupJoinRequest> GroupDaoImplT<DbMgr>::FindJoinRequestById(int64_t id) {
    auto res = db_.DB().template query_s<GroupJoinRequest>("id=?", id);
    if (res.empty())
        return std::nullopt;
    return res[0];
}

template <typename DbMgr>
bool GroupDaoImplT<DbMgr>::UpdateJoinRequestStatus(int64_t id, int status) {
    std::string now = NowUtcStr();
    std::string sql = "UPDATE group_join_requests SET status = " + std::to_string(status) +
                      ", updated_at = '" + now + "' WHERE id = " + std::to_string(id);
    return db_.DB().execute(sql);
}

// 显式实例化
template class GroupDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class GroupDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
