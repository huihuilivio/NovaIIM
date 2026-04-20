#include "contact_vm.h"
#include <service/friend_service.h>
#include <service/profile_service.h>

namespace nova::client {

ContactVM::ContactVM(FriendService& friend_svc, ProfileService& profile)
    : friend_(friend_svc), profile_(profile) {}
ContactVM::~ContactVM() = default;

// ---- 好友 ----

void ContactVM::AddFriend(const std::string& target_uid, const std::string& remark,
                          AddFriendCallback cb) {
    friend_.AddFriend(target_uid, remark, std::move(cb));
}

void ContactVM::HandleFriendRequest(int64_t request_id, int action, HandleFriendCallback cb) {
    friend_.HandleFriendRequest(request_id, action, std::move(cb));
}

void ContactVM::DeleteFriend(const std::string& target_uid, ResultCallback cb) {
    friend_.DeleteFriend(target_uid, std::move(cb));
}

void ContactVM::BlockFriend(const std::string& target_uid, ResultCallback cb) {
    friend_.BlockFriend(target_uid, std::move(cb));
}

void ContactVM::UnblockFriend(const std::string& target_uid, ResultCallback cb) {
    friend_.UnblockFriend(target_uid, std::move(cb));
}

void ContactVM::GetFriendList(FriendListCallback cb) {
    friend_.GetFriendList(std::move(cb));
}

void ContactVM::GetFriendRequests(int page, int page_size, FriendRequestsCallback cb) {
    friend_.GetFriendRequests(page, page_size, std::move(cb));
}

// ---- 用户资料 ----

void ContactVM::GetUserProfile(const std::string& target_uid, UserProfileCallback cb) {
    profile_.GetUserProfile(target_uid, std::move(cb));
}

void ContactVM::SearchUser(const std::string& keyword, SearchUserCallback cb) {
    profile_.SearchUser(keyword, std::move(cb));
}

void ContactVM::UpdateProfile(const std::string& nickname, const std::string& avatar,
                               const std::string& file_hash, ResultCallback cb) {
    profile_.UpdateProfile(nickname, avatar, file_hash, std::move(cb));
}

// ---- 事件监听 ----

void ContactVM::OnFriendNotify(FriendNotifyCallback cb) {
    friend_.OnNotify(std::move(cb));
}

}  // namespace nova::client
