#pragma once
// FriendService — 好友相关服务

#include <viewmodel/types.h>

#include <cstdint>
#include <functional>
#include <string>

namespace nova::client {

class ClientContext;

class FriendService {
public:
    using AddFriendCallback       = std::function<void(const AddFriendResult&)>;
    using HandleFriendCallback    = std::function<void(const HandleFriendResult&)>;
    using FriendListCallback      = std::function<void(const FriendListResult&)>;
    using FriendRequestsCallback  = std::function<void(const FriendRequestsResult&)>;
    using FriendNotifyCallback    = std::function<void(const FriendNotification&)>;

    explicit FriendService(ClientContext& ctx);
    ~FriendService();

    void AddFriend(const std::string& target_uid, const std::string& remark,
                   AddFriendCallback cb);
    void HandleFriendRequest(int64_t request_id, int action,
                             HandleFriendCallback cb);
    void DeleteFriend(const std::string& target_uid, ResultCallback cb = nullptr);
    void BlockFriend(const std::string& target_uid, ResultCallback cb = nullptr);
    void UnblockFriend(const std::string& target_uid, ResultCallback cb = nullptr);
    void GetFriendList(FriendListCallback cb);
    void GetFriendRequests(int page, int page_size, FriendRequestsCallback cb);

    // 事件监听（传入 nullptr 可取消订阅）
    void OnNotify(FriendNotifyCallback cb);

private:
    ClientContext& ctx_;
    uint64_t on_notify_sub_id_{0};
};

}  // namespace nova::client
