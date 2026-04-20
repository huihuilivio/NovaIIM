#include "friend_service.h"
#include "service_helpers.h"

namespace nova::client {

using namespace detail;

FriendService::FriendService(ClientContext& ctx) : ctx_(ctx) {}

void FriendService::AddFriend(const std::string& target_uid, const std::string& remark,
                         AddFriendCallback cb) {
    nova::proto::AddFriendReq req;
    req.target_uid = target_uid;
    req.remark     = remark;

    auto pkt = MakePacket(nova::proto::Cmd::kAddFriend, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::AddFriendAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::AddFriendAck>& ack) {
            AddFriendResult result;
            if (ack && ack->code == 0) {
                result.success    = true;
                result.request_id = ack->request_id;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void FriendService::HandleFriendRequest(int64_t request_id, int action,
                                    HandleFriendCallback cb) {
    nova::proto::HandleFriendReqReq req;
    req.request_id = request_id;
    req.action     = action;

    auto pkt = MakePacket(nova::proto::Cmd::kHandleFriendReq, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::HandleFriendReqAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::HandleFriendReqAck>& ack) {
            HandleFriendResult result;
            if (ack && ack->code == 0) {
                result.success         = true;
                result.conversation_id = ack->conversation_id;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void FriendService::DeleteFriend(const std::string& target_uid, ResultCallback cb) {
    nova::proto::DeleteFriendReq req;
    req.target_uid = target_uid;

    auto pkt = MakePacket(nova::proto::Cmd::kDeleteFriend, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::DeleteFriendAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::DeleteFriendAck>& ack) {
            if (!ack) { if (cb) cb({.success = false, .msg = "deserialize error"}); return; }
            if (cb) cb({.success = (ack->code == 0), .msg = ack->msg});
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void FriendService::BlockFriend(const std::string& target_uid, ResultCallback cb) {
    nova::proto::BlockFriendReq req;
    req.target_uid = target_uid;

    auto pkt = MakePacket(nova::proto::Cmd::kBlockFriend, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::BlockFriendAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::BlockFriendAck>& ack) {
            if (!ack) { if (cb) cb({.success = false, .msg = "deserialize error"}); return; }
            if (cb) cb({.success = (ack->code == 0), .msg = ack->msg});
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void FriendService::UnblockFriend(const std::string& target_uid, ResultCallback cb) {
    nova::proto::UnblockFriendReq req;
    req.target_uid = target_uid;

    auto pkt = MakePacket(nova::proto::Cmd::kUnblockFriend, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::UnblockFriendAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::UnblockFriendAck>& ack) {
            if (!ack) { if (cb) cb({.success = false, .msg = "deserialize error"}); return; }
            if (cb) cb({.success = (ack->code == 0), .msg = ack->msg});
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void FriendService::GetFriendList(FriendListCallback cb) {
    nova::proto::GetFriendListReq req;

    auto pkt = MakePacket(nova::proto::Cmd::kGetFriendList, ctx_.NextSeq(), req);

    SendRequest<nova::proto::GetFriendListAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::GetFriendListAck>& ack) {
            FriendListResult result;
            if (ack && ack->code == 0) {
                result.success = true;
                result.friends.reserve(ack->friends.size());
                for (const auto& f : ack->friends) {
                    result.friends.push_back({
                        .uid             = f.uid,
                        .nickname        = f.nickname,
                        .avatar          = f.avatar,
                        .conversation_id = f.conversation_id,
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

void FriendService::GetFriendRequests(int page, int page_size, FriendRequestsCallback cb) {
    nova::proto::GetFriendRequestsReq req;
    req.page      = page;
    req.page_size = page_size;

    auto pkt = MakePacket(nova::proto::Cmd::kGetFriendRequests, ctx_.NextSeq(), req);

    SendRequest<nova::proto::GetFriendRequestsAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::GetFriendRequestsAck>& ack) {
            FriendRequestsResult result;
            if (ack && ack->code == 0) {
                result.success = true;
                result.total   = ack->total;
                result.requests.reserve(ack->requests.size());
                for (const auto& r : ack->requests) {
                    result.requests.push_back({
                        .request_id    = r.request_id,
                        .from_uid      = r.from_uid,
                        .from_nickname = r.from_nickname,
                        .from_avatar   = r.from_avatar,
                        .remark        = r.remark,
                        .created_at    = r.created_at,
                        .status        = r.status,
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

void FriendService::OnNotify(FriendNotifyCallback cb) {
    ctx_.Events().subscribe<nova::proto::FriendNotifyMsg>("FriendNotify",
        [cb](const nova::proto::FriendNotifyMsg& n) {
            cb({
                .notify_type      = n.notify_type,
                .from_uid         = n.from_uid,
                .from_nickname    = n.from_nickname,
                .from_avatar      = n.from_avatar,
                .remark           = n.remark,
                .request_id       = n.request_id,
                .conversation_id  = n.conversation_id,
            });
        });
}

}  // namespace nova::client
