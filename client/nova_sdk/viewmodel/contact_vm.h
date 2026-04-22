#pragma once
// ContactVM — 联系人 ViewModel（好友 + 用户资料）

#include <export.h>
#include <viewmodel/observable.h>
#include <viewmodel/types.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace nova::client {

class FriendService;
class ProfileService;

class NOVA_SDK_API ContactVM {
public:
    // Friend callbacks
    using AddFriendCallback       = std::function<void(const AddFriendResult&)>;
    using HandleFriendCallback    = std::function<void(const HandleFriendResult&)>;
    using FriendListCallback      = std::function<void(const FriendListResult&)>;
    using FriendRequestsCallback  = std::function<void(const FriendRequestsResult&)>;
    using FriendNotifyCallback    = std::function<void(const FriendNotification&)>;
    // Profile callbacks
    using UserProfileCallback     = std::function<void(const UserProfile&)>;
    using SearchUserCallback      = std::function<void(const SearchUserResult&)>;

    ContactVM(FriendService& friend_svc, ProfileService& profile);
    ~ContactVM();

    // ---- 好友 ----
    void AddFriend(const std::string& target_uid, const std::string& remark,
                   AddFriendCallback cb);
    void HandleFriendRequest(int64_t request_id, int action, HandleFriendCallback cb);
    void DeleteFriend(const std::string& target_uid, ResultCallback cb = nullptr);
    void BlockFriend(const std::string& target_uid, ResultCallback cb = nullptr);
    void UnblockFriend(const std::string& target_uid, ResultCallback cb = nullptr);
    void GetFriendList(FriendListCallback cb);
    void GetFriendRequests(int page, int page_size, FriendRequestsCallback cb);

    // ---- 用户资料 ----
    void GetUserProfile(const std::string& target_uid, UserProfileCallback cb);
    void SearchUser(const std::string& keyword, SearchUserCallback cb);
    void UpdateProfile(const std::string& nickname, const std::string& avatar,
                       const std::string& file_hash, ResultCallback cb);

    // ---- 事件监听 ----
    void OnFriendNotify(FriendNotifyCallback cb);

    /// 好友列表（Observable）
    Observable<std::vector<FriendEntry>>& Friends() { return friends_; }

private:
    FriendService& friend_;
    ProfileService& profile_;
    Observable<std::vector<FriendEntry>> friends_;
    std::shared_ptr<std::atomic<bool>> alive_{std::make_shared<std::atomic<bool>>(true)};
};

}  // namespace nova::client
