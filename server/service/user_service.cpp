#include "user_service.h"
#include "../net/conn_manager.h"
#include "../core/server_context.h"
#include "../core/logger.h"
#include "../admin/password_utils.h"
#include "../dao/user_dao.h"

#include <hv/json.hpp>

namespace nova {

static constexpr const char* kLogTag = "UserService";

void UserService::SendReply(ConnectionPtr& conn, Cmd cmd, uint32_t seq, uint64_t uid, const std::string& body) {
    Packet ack;
    ack.cmd = static_cast<uint16_t>(cmd);
    ack.seq = seq;
    ack.uid = uid;
    ack.body = body;
    conn->Send(ack);
    ctx_.incr_messages_out();
}

void UserService::HandleLogin(ConnectionPtr conn, Packet& pkt) {
    uint32_t seq = pkt.seq;

    // 1. 解码 body → JSON
    auto js = nlohmann::json::parse(pkt.body, nullptr, false);
    if (js.is_discarded()) {
        nlohmann::json err = {{"code", 1}, {"msg", "invalid json"}};
        SendReply(conn, Cmd::kLoginAck, seq, 0, err.dump());
        conn->Close();
        return;
    }

    std::string uid       = js.value("uid", "");
    std::string password  = js.value("password", "");
    std::string device_id = js.value("device_id", "");

    if (uid.empty()) {
        nlohmann::json err = {{"code", 1}, {"msg", "uid is required"}};
        SendReply(conn, Cmd::kLoginAck, seq, 0, err.dump());
        conn->Close();
        return;
    }

    // 2. 查询用户
    auto user_opt = ctx_.dao().User().FindByUid(uid);
    if (!user_opt) {
        nlohmann::json err = {{"code", 3}, {"msg", "user not found"}};
        SendReply(conn, Cmd::kLoginAck, seq, 0, err.dump());
        conn->Close();
        return;
    }

    auto& user = *user_opt;

    // 3. 检查用户状态
    if (user.status == 2) {
        nlohmann::json err = {{"code", 4}, {"msg", "user is banned"}};
        SendReply(conn, Cmd::kLoginAck, seq, 0, err.dump());
        conn->Close();
        return;
    }

    // 4. 验证密码
    if (!PasswordUtils::Verify(password, user.password_hash)) {
        nlohmann::json err = {{"code", 2}, {"msg", "wrong password"}};
        SendReply(conn, Cmd::kLoginAck, seq, 0, err.dump());
        conn->Close();
        return;
    }

    // 5. 设置连接状态
    conn->set_user_id(user.id);
    if (!device_id.empty()) {
        conn->set_device_id(device_id);
    }

    // 6. 注册到连接管理器
    ConnManager::Instance().Add(user.id, conn);
    ctx_.add_online_user();

    NOVA_NLOG_INFO(kLogTag, "user {} (id={}) logged in, device={}", uid, user.id, device_id);

    // 7. 返回 LoginAck
    nlohmann::json resp = {
        {"code", 0},
        {"msg", "ok"},
        {"user_id", user.id},
        {"nickname", user.nickname},
        {"avatar", user.avatar},
    };
    SendReply(conn, Cmd::kLoginAck, seq, static_cast<uint64_t>(user.id), resp.dump());
}

void UserService::HandleLogout(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id != 0) {
        ConnManager::Instance().Remove(user_id, conn.get());
        ctx_.remove_online_user();
        NOVA_NLOG_INFO(kLogTag, "user id={} logged out", user_id);
    }

    nlohmann::json resp = {{"code", 0}, {"msg", "goodbye"}};
    SendReply(conn, Cmd::kLogout, pkt.seq, static_cast<uint64_t>(user_id), resp.dump());
    conn->Close();
}

void UserService::HandleHeartbeat(ConnectionPtr conn, Packet& pkt) {
    // 回复心跳 ACK，使用服务端认证后的 user_id
    nlohmann::json resp = {{"code", 0}};
    SendReply(conn, Cmd::kHeartbeatAck, pkt.seq, static_cast<uint64_t>(conn->user_id()), resp.dump());
}

} // namespace nova
