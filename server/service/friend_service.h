#pragma once

#include "service_base.h"

namespace nova {

class FriendService : public ServiceBase {
public:
    explicit FriendService(ServerContext& ctx) : ServiceBase(ctx) {}

    void HandleAddFriend(ConnectionPtr conn, Packet& pkt);
    void HandleRequest(ConnectionPtr conn, Packet& pkt);
    void HandleDeleteFriend(ConnectionPtr conn, Packet& pkt);
    void HandleBlock(ConnectionPtr conn, Packet& pkt);
    void HandleUnblock(ConnectionPtr conn, Packet& pkt);
    void HandleGetFriendList(ConnectionPtr conn, Packet& pkt);
    void HandleGetRequests(ConnectionPtr conn, Packet& pkt);
};

}  // namespace nova
