#pragma once

#include "../group_dao.h"

namespace nova {

class SqliteDbManager;

template <typename DbMgr>
class GroupDaoImplT : public GroupDao {
public:
    explicit GroupDaoImplT(DbMgr& db) : db_(db) {}

    bool InsertGroup(Group& group) override;
    std::optional<Group> FindByConversationId(int64_t conversation_id) override;
    bool UpdateGroup(const Group& group) override;
    bool DeleteByConversationId(int64_t conversation_id) override;
    std::vector<Group> FindGroupsByUser(int64_t user_id) override;

    bool InsertJoinRequest(GroupJoinRequest& req) override;
    std::optional<GroupJoinRequest> FindPendingJoinRequest(int64_t conversation_id, int64_t user_id) override;
    std::optional<GroupJoinRequest> FindJoinRequestById(int64_t id) override;
    bool UpdateJoinRequestStatus(int64_t id, int status) override;

private:
    DbMgr& db_;
};

}  // namespace nova
