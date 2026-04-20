#include "group_vm.h"
#include <service/group_service.h>

namespace nova::client {

GroupVM::GroupVM(GroupService& group) : group_(group) {}
GroupVM::~GroupVM() = default;

void GroupVM::CreateGroup(const std::string& name, const std::string& avatar,
                          const std::vector<int64_t>& member_ids, CreateGroupCallback cb) {
    group_.CreateGroup(name, avatar, member_ids, std::move(cb));
}

void GroupVM::DismissGroup(int64_t conversation_id, ResultCallback cb) {
    group_.DismissGroup(conversation_id, std::move(cb));
}

void GroupVM::JoinGroup(int64_t conversation_id, const std::string& remark, ResultCallback cb) {
    group_.JoinGroup(conversation_id, remark, std::move(cb));
}

void GroupVM::HandleJoinRequest(int64_t request_id, int action, ResultCallback cb) {
    group_.HandleJoinRequest(request_id, action, std::move(cb));
}

void GroupVM::LeaveGroup(int64_t conversation_id, ResultCallback cb) {
    group_.LeaveGroup(conversation_id, std::move(cb));
}

void GroupVM::KickMember(int64_t conversation_id, int64_t target_user_id, ResultCallback cb) {
    group_.KickMember(conversation_id, target_user_id, std::move(cb));
}

void GroupVM::GetGroupInfo(int64_t conversation_id, GroupInfoCallback cb) {
    group_.GetGroupInfo(conversation_id, std::move(cb));
}

void GroupVM::UpdateGroup(int64_t conversation_id, const std::string& name,
                          const std::string& avatar, const std::string& notice,
                          ResultCallback cb) {
    group_.UpdateGroup(conversation_id, name, avatar, notice, std::move(cb));
}

void GroupVM::GetGroupMembers(int64_t conversation_id, GroupMembersCallback cb) {
    group_.GetGroupMembers(conversation_id, std::move(cb));
}

void GroupVM::GetMyGroups(MyGroupsCallback cb) {
    group_.GetMyGroups(std::move(cb));
}

void GroupVM::SetMemberRole(int64_t conversation_id, int64_t target_user_id,
                            int role, ResultCallback cb) {
    group_.SetMemberRole(conversation_id, target_user_id, role, std::move(cb));
}

void GroupVM::OnNotify(GroupNotifyCallback cb) {
    group_.OnNotify(std::move(cb));
}

}  // namespace nova::client
