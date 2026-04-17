#include "friend_dao_impl.h"
#include "../sqlite3/sqlite_db_manager.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "../mysql/mysql_db_manager.h"
#endif

namespace nova {

// ---- FriendRequest ----

template <typename DbMgr>
bool FriendDaoImplT<DbMgr>::InsertRequest(FriendRequest& req) {
    auto id = db_.DB().get_insert_id_after_insert(req);
    if (id == 0) return false;
    req.id = static_cast<int64_t>(id);
    return true;
}

template <typename DbMgr>
std::optional<FriendRequest> FriendDaoImplT<DbMgr>::FindRequestById(int64_t id) {
    auto res = db_.DB().template query_s<FriendRequest>("id=?", id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

template <typename DbMgr>
std::optional<FriendRequest> FriendDaoImplT<DbMgr>::FindPendingRequest(int64_t from_id, int64_t to_id) {
    auto res = db_.DB().template query_s<FriendRequest>(
        "from_id=? AND to_id=? AND status=0", from_id, to_id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

template <typename DbMgr>
bool FriendDaoImplT<DbMgr>::UpdateRequestStatus(int64_t id, int status) {
    std::string sql = "UPDATE friend_requests SET status = " + std::to_string(status) +
                      ", updated_at = datetime('now') WHERE id = " + std::to_string(id);
    return db_.DB().execute(sql);
}

template <typename DbMgr>
FriendRequestPage FriendDaoImplT<DbMgr>::GetRequestsByUser(int64_t user_id, int32_t offset, int32_t limit) {
    FriendRequestPage page;

    // Count total
    auto count_res = db_.DB().template query_s<std::tuple<int64_t>>(
        "SELECT count(*) FROM friend_requests WHERE to_id = " + std::to_string(user_id));
    if (!count_res.empty()) {
        page.total = std::get<0>(count_res[0]);
    }

    // Query page — 使用 raw SQL 避免 ormpp WHERE-stripping bug
    std::string sql = "SELECT * FROM friend_requests WHERE to_id = " + std::to_string(user_id) +
                      " ORDER BY id DESC LIMIT " + std::to_string(limit) +
                      " OFFSET " + std::to_string(offset);
    page.items = db_.DB().template query_s<FriendRequest>(sql);
    return page;
}

// ---- Friendship ----

template <typename DbMgr>
bool FriendDaoImplT<DbMgr>::InsertFriendship(Friendship& f) {
    auto id = db_.DB().get_insert_id_after_insert(f);
    if (id == 0) return false;
    f.id = static_cast<int64_t>(id);
    return true;
}

template <typename DbMgr>
std::optional<Friendship> FriendDaoImplT<DbMgr>::FindFriendship(int64_t user_id, int64_t friend_id) {
    auto res = db_.DB().template query_s<Friendship>(
        "user_id=? AND friend_id=?", user_id, friend_id);
    if (res.empty()) return std::nullopt;
    return res[0];
}

template <typename DbMgr>
bool FriendDaoImplT<DbMgr>::UpdateFriendshipStatus(int64_t user_id, int64_t friend_id, int status) {
    std::string sql = "UPDATE friendships SET status = " + std::to_string(status) +
                      ", updated_at = datetime('now') WHERE user_id = " + std::to_string(user_id) +
                      " AND friend_id = " + std::to_string(friend_id);
    return db_.DB().execute(sql);
}

template <typename DbMgr>
std::vector<Friendship> FriendDaoImplT<DbMgr>::GetFriendsByUser(int64_t user_id) {
    return db_.DB().template query_s<Friendship>(
        "user_id=? AND status=1", user_id);
}

// 显式实例化
template class FriendDaoImplT<SqliteDbManager>;
#ifdef ORMPP_ENABLE_MYSQL
template class FriendDaoImplT<MysqlDbManager>;
#endif

}  // namespace nova
