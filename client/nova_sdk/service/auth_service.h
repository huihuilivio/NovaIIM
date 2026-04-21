#pragma once
// AuthService — 认证相关服务

#include <viewmodel/types.h>

#include <functional>
#include <string>

namespace nova::client {

class ClientContext;

class AuthService {
public:
    using LoginCallback    = std::function<void(const LoginResult&)>;
    using RegisterCallback = std::function<void(const RegisterResult&)>;

    explicit AuthService(ClientContext& ctx);

    void Login(const std::string& email, const std::string& password, LoginCallback cb);
    void Register(const std::string& email, const std::string& nickname,
                  const std::string& password, RegisterCallback cb);
    void Logout();
    bool IsLoggedIn() const;
    std::string Uid() const;

private:
    ClientContext& ctx_;
};

}  // namespace nova::client
