#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "../model/types.h"

namespace nova {

struct FriendRequestPage {
    std::vector<FriendRequest> items;
    int64_t total = 0;
};

class FriendDao {
public:
    virtual ~FriendDao() = default;

    // ---- FriendRequest ----
    virtual bool InsertRequest(FriendRequest& req) = 0;
    virtual std::optional<FriendRequest> FindRequestById(int64_t id) = 0;
    virtual std::optional<FriendRequest> FindPendingRequest(int64_t from_id, int64_t to_id) = 0;
    virtual bool UpdateRequestStatus(int64_t id, int status) = 0;
    virtual FriendRequestPage GetRequestsByUser(int64_t user_id, int32_t offset, int32_t limit) = 0;

    // ---- Friendship ----
    virtual bool InsertFriendship(Friendship& f) = 0;
    virtual std::optional<Friendship> FindFriendship(int64_t user_id, int64_t friend_id) = 0;
    virtual bool UpdateFriendshipStatus(int64_t user_id, int64_t friend_id, int status) = 0;
    virtual std::vector<Friendship> GetFriendsByUser(int64_t user_id) = 0;
};

}  // namespace nova
