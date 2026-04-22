#pragma once
// AuthService — 认证相关服务

#include <viewmodel/types.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace nova::client {

class ClientContext;

class AuthService {
public:
    using LoginCallback    = std::function<void(const LoginResult&)>;
    using RegisterCallback = std::function<void(const RegisterResult&)>;

    explicit AuthService(ClientContext& ctx);
    ~AuthService();

    void Login(const std::string& email, const std::string& password, LoginCallback cb);
    void Register(const std::string& email, const std::string& nickname,
                  const std::string& password, RegisterCallback cb);
    void Logout();
    bool IsLoggedIn() const;
    std::string Uid() const;

private:
    ClientContext& ctx_;
    // 生命周期守卫：在 dtor 中置为 false，异步回调由 lambda 捕获后判断是否仍可访问 this
    std::shared_ptr<std::atomic<bool>> alive_{std::make_shared<std::atomic<bool>>(true)};
};

}  // namespace nova::client
