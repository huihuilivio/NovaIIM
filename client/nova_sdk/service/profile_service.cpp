#include "profile_service.h"
#include "service_helpers.h"

namespace nova::client {

using namespace detail;

ProfileService::ProfileService(ClientContext& ctx) : ctx_(ctx) {}

void ProfileService::GetUserProfile(const std::string& target_uid, UserProfileCallback cb) {
    nova::proto::GetUserProfileReq req;
    req.target_uid = target_uid;

    auto pkt = MakePacket(nova::proto::Cmd::kGetUserProfile, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::GetUserProfileAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::GetUserProfileAck>& ack) {
            UserProfile result;
            if (ack && ack->code == 0) {
                result.success  = true;
                result.uid      = ack->uid;
                result.nickname = ack->nickname;
                result.avatar   = ack->avatar;
                result.email    = ack->email;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void ProfileService::SearchUser(const std::string& keyword, SearchUserCallback cb) {
    nova::proto::SearchUserReq req;
    req.keyword = keyword;

    auto pkt = MakePacket(nova::proto::Cmd::kSearchUser, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::SearchUserAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::SearchUserAck>& ack) {
            SearchUserResult result;
            if (ack && ack->code == 0) {
                result.success = true;
                result.users.reserve(ack->users.size());
                for (const auto& u : ack->users) {
                    result.users.push_back({
                        .uid      = u.uid,
                        .nickname = u.nickname,
                        .avatar   = u.avatar,
                    });
                }
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void ProfileService::UpdateProfile(const std::string& nickname, const std::string& avatar,
                               const std::string& file_hash, ResultCallback cb) {
    nova::proto::UpdateProfileReq req;
    req.nickname  = nickname;
    req.avatar    = avatar;
    req.file_hash = file_hash;

    auto pkt = MakePacket(nova::proto::Cmd::kUpdateProfile, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::UpdateProfileAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::UpdateProfileAck>& ack) {
            if (!ack) { if (cb) cb({.success = false, .msg = "deserialize error"}); return; }
            if (cb) cb({.success = (ack->code == 0), .msg = ack->msg});
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

}  // namespace nova::client
