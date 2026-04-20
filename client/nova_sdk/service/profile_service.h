#pragma once
// ProfileService — 用户资料相关服务

#include <viewmodel/types.h>

#include <functional>
#include <string>

namespace nova::client {

class ClientContext;

class ProfileService {
public:
    using UserProfileCallback = std::function<void(const UserProfile&)>;
    using SearchUserCallback  = std::function<void(const SearchUserResult&)>;

    explicit ProfileService(ClientContext& ctx);

    void GetUserProfile(const std::string& target_uid, UserProfileCallback cb);
    void SearchUser(const std::string& keyword, SearchUserCallback cb);
    void UpdateProfile(const std::string& nickname, const std::string& avatar,
                       const std::string& file_hash, ResultCallback cb);

private:
    ClientContext& ctx_;
};

}  // namespace nova::client
