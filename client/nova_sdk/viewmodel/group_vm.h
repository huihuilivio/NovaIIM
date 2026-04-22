#pragma once
// GroupVM — 群组管理 ViewModel

#include <export.h>
#include <viewmodel/observable.h>
#include <viewmodel/types.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nova::client {

class GroupService;

class NOVA_SDK_API GroupVM {
public:
    using CreateGroupCallback  = std::function<void(const CreateGroupResult&)>;
    using GroupInfoCallback    = std::function<void(const GroupInfo&)>;
    using GroupMembersCallback = std::function<void(const GroupMembersResult&)>;
    using MyGroupsCallback     = std::function<void(const MyGroupsResult&)>;
    using GroupNotifyCallback  = std::function<void(const GroupNotification&)>;

    explicit GroupVM(GroupService& group);
    ~GroupVM();

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

    // 事件监听
    void OnNotify(GroupNotifyCallback cb);

    /// 我的群组列表（Observable）
    Observable<std::vector<MyGroup>>& Groups() { return groups_; }

private:
    GroupService& group_;
    Observable<std::vector<MyGroup>> groups_;
    std::shared_ptr<std::atomic<bool>> alive_{std::make_shared<std::atomic<bool>>(true)};
};

}  // namespace nova::client
