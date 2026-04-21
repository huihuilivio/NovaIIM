#include "login_vm.h"
#include <service/auth_service.h>

namespace nova::client {

LoginVM::LoginVM(AuthService& auth) : auth_(auth) {}
LoginVM::~LoginVM() { *alive_ = false; }

void LoginVM::Login(const std::string& email, const std::string& password, LoginCallback cb) {
    std::weak_ptr<bool> weak = alive_;
    auth_.Login(email, password, [this, weak, cb = std::move(cb)](const LoginResult& r) {
        auto alive = weak.lock();
        if (!alive) return;
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
