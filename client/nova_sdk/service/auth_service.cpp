#include "auth_service.h"
#include "service_helpers.h"

namespace nova::client {

using namespace detail;

AuthService::AuthService(ClientContext& ctx) : ctx_(ctx) {}

void AuthService::Login(const std::string& email, const std::string& password, LoginCallback cb) {
    nova::proto::LoginReq req;
    req.email       = email;
    req.password    = password;
    req.device_id   = ctx_.Config().device_id;
    req.device_type = ctx_.Config().device_type;

    auto pkt = MakePacket(nova::proto::Cmd::kLogin, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::LoginAck>(ctx_, pkt,
        [this, cb](const std::optional<nova::proto::LoginAck>& ack) {
            LoginResult result;
            if (ack && ack->code == 0) {
                ctx_.SetAuthenticated(ack->uid);
                result.success  = true;
                result.uid      = ack->uid;
                result.nickname = ack->nickname;
                result.avatar   = ack->avatar;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "login timeout"}); }
    );
}

void AuthService::Register(const std::string& email, const std::string& nickname,
                      const std::string& password, RegisterCallback cb) {
    nova::proto::RegisterReq req;
    req.email    = email;
    req.nickname = nickname;
    req.password = password;

    auto pkt = MakePacket(nova::proto::Cmd::kRegister, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::RegisterAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RegisterAck>& ack) {
            RegisterResult result;
            if (ack && ack->code == 0) {
                result.success = true;
                result.uid     = ack->uid;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "register timeout"}); }
    );
}

void AuthService::Logout() {
    auto pkt = MakePacket(nova::proto::Cmd::kLogout, ctx_.NextSeq());
    ctx_.SendPacket(pkt);
    ctx_.Shutdown();
}

bool AuthService::IsLoggedIn() const {
    return ctx_.IsLoggedIn();
}

const std::string& AuthService::Uid() const {
    return ctx_.Uid();
}

}  // namespace nova::client
