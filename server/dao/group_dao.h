#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "../model/types.h"

namespace nova {

class GroupDao {
public:
    virtual ~GroupDao() = default;

    virtual bool InsertGroup(Group& group) = 0;
    virtual std::optional<Group> FindByConversationId(int64_t conversation_id) = 0;
    virtual bool UpdateGroup(const Group& group) = 0;
    virtual bool DeleteByConversationId(int64_t conversation_id) = 0;
    virtual std::vector<Group> FindGroupsByUser(int64_t user_id) = 0;

    virtual bool InsertJoinRequest(GroupJoinRequest& req) = 0;
    virtual std::optional<GroupJoinRequest> FindPendingJoinRequest(int64_t conversation_id, int64_t user_id) = 0;
    virtual std::optional<GroupJoinRequest> FindJoinRequestById(int64_t id) = 0;
    virtual bool UpdateJoinRequestStatus(int64_t id, int status) = 0;
};

}  // namespace nova
