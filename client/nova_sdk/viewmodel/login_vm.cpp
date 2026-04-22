#include "login_vm.h"
#include <service/auth_service.h>

namespace nova::client {

LoginVM::LoginVM(AuthService& auth) : auth_(auth) {}
LoginVM::~LoginVM() { alive_->store(false); }

void LoginVM::Login(const std::string& email, const std::string& password, LoginCallback cb) {
    auth_.Login(email, password, [this, alive = alive_, cb = std::move(cb)](const LoginResult& r) {
        if (!alive->load()) return;  // LoginVM 已销毁
        if (r.success) {
            uid_.Set(r.uid);
            logged_in_.Set(true);
        }
        if (cb) cb(r);
    });
}

void LoginVM::Register(const std::string& email, const std::string& nickname,
                       const std::string& password, RegisterCallback cb) {
    auth_.Register(email, nickname, password, std::move(cb));
}

void LoginVM::Logout() {
    auth_.Logout();
    logged_in_.Set(false);
    uid_.Set({});
}

}  // namespace nova::client
