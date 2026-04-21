#pragma once
// LoginVM — 登录/注册 ViewModel

#include <export.h>
#include <viewmodel/observable.h>
#include <viewmodel/types.h>

#include <functional>
#include <memory>
#include <string>

namespace nova::client {

class AuthService;

class NOVA_SDK_API LoginVM {
public:
    using LoginCallback    = std::function<void(const LoginResult&)>;
    using RegisterCallback = std::function<void(const RegisterResult&)>;

    explicit LoginVM(AuthService& auth);
    ~LoginVM();

    void Login(const std::string& email, const std::string& password,
               LoginCallback cb = nullptr);
    void Register(const std::string& email, const std::string& nickname,
                  const std::string& password, RegisterCallback cb = nullptr);
    void Logout();

    /// 登录状态（Observable）
    Observable<bool>& LoggedIn() { return logged_in_; }

    /// 当前用户 UID（Observable）
    Observable<std::string>& Uid() { return uid_; }

private:
    AuthService& auth_;
    Observable<bool> logged_in_{false};
    Observable<std::string> uid_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

}  // namespace nova::client
