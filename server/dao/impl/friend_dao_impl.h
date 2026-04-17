#pragma once

#include "../friend_dao.h"

namespace nova {

class SqliteDbManager;

template <typename DbMgr>
class FriendDaoImplT : public FriendDao {
public:
    explicit FriendDaoImplT(DbMgr& db) : db_(db) {}

    bool InsertRequest(FriendRequest& req) override;
    std::optional<FriendRequest> FindRequestById(int64_t id) override;
    std::optional<FriendRequest> FindPendingRequest(int64_t from_id, int64_t to_id) override;
    bool UpdateRequestStatus(int64_t id, int status) override;
    FriendRequestPage GetRequestsByUser(int64_t user_id, int32_t offset, int32_t limit) override;

    bool InsertFriendship(Friendship& f) override;
    std::optional<Friendship> FindFriendship(int64_t user_id, int64_t friend_id) override;
    bool UpdateFriendshipStatus(int64_t user_id, int64_t friend_id, int status) override;
    std::vector<Friendship> GetFriendsByUser(int64_t user_id) override;

private:
    DbMgr& db_;
};

}  // namespace nova
