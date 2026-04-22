#pragma once
// GroupService — 群组管理相关服务

#include <viewmodel/types.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nova::client {

class ClientContext;

class GroupService {
public:
    using CreateGroupCallback  = std::function<void(const CreateGroupResult&)>;
    using GroupInfoCallback    = std::function<void(const GroupInfo&)>;
    using GroupMembersCallback = std::function<void(const GroupMembersResult&)>;
    using MyGroupsCallback     = std::function<void(const MyGroupsResult&)>;
    using GroupNotifyCallback  = std::function<void(const GroupNotification&)>;

    explicit GroupService(ClientContext& ctx);
    ~GroupService();

    void CreateGroup(const std::string& name, const std::string& avatar,
                     const std::vector<int64_t>& member_ids, CreateGroupCallback cb);
    void DismissGroup(int64_t conversation_id, ResultCallback cb = nullptr);
    void JoinGroup(int64_t conversation_id, const std::string& remark,
                   ResultCallback cb = nullptr);
    void HandleJoinRequest(int64_t request_id, int action,
                           ResultCallback cb = nullptr);
    void LeaveGroup(int64_t conversation_id, ResultCallback cb = nullptr);
    void KickMember(int64_t conversation_id, int64_t target_user_id,
                    ResultCallback cb = nullptr);
    void GetGroupInfo(int64_t conversation_id, GroupInfoCallback cb);
    void UpdateGroup(int64_t conversation_id, const std::string& name,
                     const std::string& avatar, const std::string& notice,
                     ResultCallback cb = nullptr);
    void GetGroupMembers(int64_t conversation_id, GroupMembersCallback cb);
    void GetMyGroups(MyGroupsCallback cb);
    void SetMemberRole(int64_t conversation_id, int64_t target_user_id,
                       int role, ResultCallback cb = nullptr);

    // 事件监听（传入 nullptr 可取消订阅）
    void OnNotify(GroupNotifyCallback cb);

private:
    ClientContext& ctx_;
    uint64_t on_notify_sub_id_{0};
};

}  // namespace nova::client
