#pragma once

#include "service_base.h"

namespace nova {

class GroupService : public ServiceBase {
public:
    explicit GroupService(ServerContext& ctx) : ServiceBase(ctx) {}

    void HandleCreateGroup(ConnectionPtr conn, Packet& pkt);
    void HandleDismissGroup(ConnectionPtr conn, Packet& pkt);
    void HandleJoinGroup(ConnectionPtr conn, Packet& pkt);
    void HandleJoinReq(ConnectionPtr conn, Packet& pkt);
    void HandleLeaveGroup(ConnectionPtr conn, Packet& pkt);
    void HandleKickMember(ConnectionPtr conn, Packet& pkt);
    void HandleGetGroupInfo(ConnectionPtr conn, Packet& pkt);
    void HandleUpdateGroup(ConnectionPtr conn, Packet& pkt);
    void HandleGetGroupMembers(ConnectionPtr conn, Packet& pkt);
    void HandleGetMyGroups(ConnectionPtr conn, Packet& pkt);
    void HandleSetMemberRole(ConnectionPtr conn, Packet& pkt);

private:
    void SendGroupNotify(int64_t conversation_id, int64_t exclude_user_id,
                         const proto::GroupNotifyMsg& notify);
};

}  // namespace nova
