#include "user_service.h"
#include "../core/server_context.h"
#include "../core/logger.h"
#include "../admin/password_utils.h"
#include "../dao/user_dao.h"

namespace nova {

static constexpr const char* kLogTag = "UserService";

template <typename T>
void UserService::SendPacket(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const T& body) {
    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(cmd);
    pkt.seq = seq;
    pkt.uid = uid;
    pkt.body = proto::Serialize(body);
    conn->Send(pkt);
    ctx_.incr_messages_out();
}

void UserService::HandleLogin(ConnectionPtr conn, Packet& pkt) {
    uint32_t seq = pkt.seq;

    // 1. 反序列化 body → LoginReq
    auto req = proto::Deserialize<proto::LoginReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{1, "invalid body"});
        conn->Close();
        return;
    }

    if (req->uid.empty()) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{1, "uid is required"});
        conn->Close();
        return;
    }

    // 2. 查询用户
    auto user_opt = ctx_.dao().User().FindByUid(req->uid);
    if (!user_opt) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{3, "user not found"});
        conn->Close();
        return;
    }

    auto& user = *user_opt;

    // 3. 检查用户状态
    if (user.status == 2) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{4, "user is banned"});
        conn->Close();
        return;
    }

    // 4. 验证密码
    if (!PasswordUtils::Verify(req->password, user.password_hash)) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{2, "wrong password"});
        conn->Close();
        return;
    }

    // 5. 设置连接状态
    conn->set_user_id(user.id);
    if (!req->device_id.empty()) {
        conn->set_device_id(req->device_id);
    }

    // 6. 注册到连接管理器
    ctx_.conn_manager().Add(user.id, conn);
    ctx_.add_online_user();

    NOVA_NLOG_INFO(kLogTag, "user {} (id={}) logged in, device={}",
                   req->uid, user.id, req->device_id);

    // 7. 返回 LoginAck
    SendPacket(conn, Cmd::kLoginAck, seq, static_cast<uint64_t>(user.id),
               proto::LoginAck{0, "ok", user.id, user.nickname, user.avatar});
}

void UserService::HandleLogout(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id != 0) {
        ctx_.conn_manager().Remove(user_id, conn.get());
        ctx_.remove_online_user();
        NOVA_NLOG_INFO(kLogTag, "user id={} logged out", user_id);
    }

    SendPacket(conn, Cmd::kLogout, pkt.seq, static_cast<uint64_t>(user_id),
               proto::RspBase{0, "goodbye"});
    conn->Close();
}

void UserService::HandleHeartbeat(ConnectionPtr conn, Packet& pkt) {
    SendPacket(conn, Cmd::kHeartbeatAck, pkt.seq,
               static_cast<uint64_t>(conn->user_id()), proto::RspBase{0, {}});
}

} // namespace nova
