#include "group_service.h"
#include "service_helpers.h"

namespace nova::client {

using namespace detail;

GroupService::GroupService(ClientContext& ctx) : ctx_(ctx) {}

void GroupService::CreateGroup(const std::string& name, const std::string& avatar,
                          const std::vector<int64_t>& member_ids, CreateGroupCallback cb) {
    nova::proto::CreateGroupReq req;
    req.name       = name;
    req.avatar     = avatar;
    req.member_ids = member_ids;

    auto pkt = MakePacket(nova::proto::Cmd::kCreateGroup, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::CreateGroupAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::CreateGroupAck>& ack) {
            CreateGroupResult result;
            if (ack && ack->code == 0) {
                result.success         = true;
                result.conversation_id = ack->conversation_id;
                result.group_id        = ack->group_id;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::DismissGroup(int64_t conversation_id, ResultCallback cb) {
    nova::proto::DismissGroupReq req;
    req.conversation_id = conversation_id;

    auto pkt = MakePacket(nova::proto::Cmd::kDismissGroup, ctx_.NextSeq(), req);

    SendRequest<nova::proto::RspBase>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RspBase>& ack) {
            if (cb) cb(ToResult(ack));
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::JoinGroup(int64_t conversation_id, const std::string& remark, ResultCallback cb) {
    nova::proto::JoinGroupReq req;
    req.conversation_id = conversation_id;
    req.remark          = remark;

    auto pkt = MakePacket(nova::proto::Cmd::kJoinGroup, ctx_.NextSeq(), req);

    SendRequest<nova::proto::RspBase>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RspBase>& ack) {
            if (cb) cb(ToResult(ack));
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::HandleJoinRequest(int64_t request_id, int action, ResultCallback cb) {
    nova::proto::HandleJoinReqReq req;
    req.request_id = request_id;
    req.action     = action;

    auto pkt = MakePacket(nova::proto::Cmd::kHandleJoinReq, ctx_.NextSeq(), req);

    SendRequest<nova::proto::RspBase>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RspBase>& ack) {
            if (cb) cb(ToResult(ack));
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::LeaveGroup(int64_t conversation_id, ResultCallback cb) {
    nova::proto::LeaveGroupReq req;
    req.conversation_id = conversation_id;

    auto pkt = MakePacket(nova::proto::Cmd::kLeaveGroup, ctx_.NextSeq(), req);

    SendRequest<nova::proto::RspBase>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RspBase>& ack) {
            if (cb) cb(ToResult(ack));
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::KickMember(int64_t conversation_id, int64_t target_user_id, ResultCallback cb) {
    nova::proto::KickMemberReq req;
    req.conversation_id = conversation_id;
    req.target_user_id  = target_user_id;

    auto pkt = MakePacket(nova::proto::Cmd::kKickMember, ctx_.NextSeq(), req);

    SendRequest<nova::proto::RspBase>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RspBase>& ack) {
            if (cb) cb(ToResult(ack));
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::GetGroupInfo(int64_t conversation_id, GroupInfoCallback cb) {
    nova::proto::GetGroupInfoReq req;
    req.conversation_id = conversation_id;

    auto pkt = MakePacket(nova::proto::Cmd::kGetGroupInfo, ctx_.NextSeq(), req);

    SendRequest<nova::proto::GetGroupInfoAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::GetGroupInfoAck>& ack) {
            GroupInfo result;
            if (ack && ack->code == 0) {
                result.success         = true;
                result.conversation_id = ack->conversation_id;
                result.name            = ack->name;
                result.avatar          = ack->avatar;
                result.owner_id        = ack->owner_id;
                result.notice          = ack->notice;
                result.member_count    = ack->member_count;
                result.created_at      = ack->created_at;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::UpdateGroup(int64_t conversation_id, const std::string& name,
                          const std::string& avatar, const std::string& notice,
                          ResultCallback cb) {
    nova::proto::UpdateGroupReq req;
    req.conversation_id = conversation_id;
    req.name            = name;
    req.avatar          = avatar;
    req.notice          = notice;

    auto pkt = MakePacket(nova::proto::Cmd::kUpdateGroup, ctx_.NextSeq(), req);

    SendRequest<nova::proto::RspBase>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RspBase>& ack) {
            if (cb) cb(ToResult(ack));
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::GetGroupMembers(int64_t conversation_id, GroupMembersCallback cb) {
    nova::proto::GetGroupMembersReq req;
    req.conversation_id = conversation_id;

    auto pkt = MakePacket(nova::proto::Cmd::kGetGroupMembers, ctx_.NextSeq(), req);

    SendRequest<nova::proto::GetGroupMembersAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::GetGroupMembersAck>& ack) {
            GroupMembersResult result;
            if (ack && ack->code == 0) {
                result.success = true;
                result.members.reserve(ack->members.size());
                for (const auto& m : ack->members) {
                    result.members.push_back({
                        .user_id   = m.user_id,
                        .uid       = m.uid,
                        .nickname  = m.nickname,
                        .avatar    = m.avatar,
                        .role      = m.role,
                        .joined_at = m.joined_at,
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

void GroupService::GetMyGroups(MyGroupsCallback cb) {
    nova::proto::GetMyGroupsReq req;

    auto pkt = MakePacket(nova::proto::Cmd::kGetMyGroups, ctx_.NextSeq(), req);

    SendRequest<nova::proto::GetMyGroupsAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::GetMyGroupsAck>& ack) {
            MyGroupsResult result;
            if (ack && ack->code == 0) {
                result.success = true;
                result.groups.reserve(ack->groups.size());
                for (const auto& g : ack->groups) {
                    result.groups.push_back({
                        .conversation_id = g.conversation_id,
                        .name            = g.name,
                        .avatar          = g.avatar,
                        .member_count    = g.member_count,
                        .my_role         = g.my_role,
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

void GroupService::SetMemberRole(int64_t conversation_id, int64_t target_user_id,
                            int role, ResultCallback cb) {
    nova::proto::SetMemberRoleReq req;
    req.conversation_id = conversation_id;
    req.target_user_id  = target_user_id;
    req.role            = role;

    auto pkt = MakePacket(nova::proto::Cmd::kSetMemberRole, ctx_.NextSeq(), req);

    SendRequest<nova::proto::RspBase>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RspBase>& ack) {
            if (cb) cb(ToResult(ack));
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void GroupService::OnNotify(GroupNotifyCallback cb) {
    ctx_.Events().subscribe<nova::proto::GroupNotifyMsg>("GroupNotify",
        [cb](const nova::proto::GroupNotifyMsg& n) {
            cb({
                .conversation_id = n.conversation_id,
                .notify_type     = n.notify_type,
                .operator_id     = n.operator_id,
                .target_ids      = n.target_ids,
                .data            = n.data,
            });
        });
}

}  // namespace nova::client
