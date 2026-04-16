#include "user_service.h"
#include "errors/user_errors.h"
#include "../core/logger.h"
#include "../admin/password_utils.h"
#include "../dao/user_dao.h"

#include <cstring>

namespace nova {

using namespace errc;

static constexpr const char* kLogTag = "UserService";

void UserService::HandleLogin(ConnectionPtr conn, Packet& pkt) {
    uint32_t seq = pkt.seq;

    // 连接已认证的情况下重新登录：先清理旧会话
    if (conn->is_authenticated()) {
        ctx_.conn_manager().Remove(conn->user_id(), conn.get());
        conn->set_user_id(0);
    }

    // 1. 反序列化 body → LoginReq
    auto req = proto::Deserialize<proto::LoginReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{kInvalidBody.code, kInvalidBody.msg});
        conn->Close();
        return;
    }

    if (req->uid.empty()) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{user::kUidRequired.code, user::kUidRequired.msg});
        conn->Close();
        return;
    }

    if (req->password.empty()) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{user::kPasswordRequired.code, user::kPasswordRequired.msg});
        conn->Close();
        return;
    }

    // 2. 频率限制检查（防暴力破解）
    if (!login_limiter_.Allow(req->uid)) {
        NOVA_NLOG_WARN(kLogTag, "login rate limited for uid={}", req->uid);
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{user::kRateLimited.code, user::kRateLimited.msg});
        conn->Close();
        return;
    }

    // 3. 查询用户
    auto user_opt = ctx_.dao().User().FindByUid(req->uid);
    if (!user_opt) {
        // 统一错误信息，防止用户枚举
        login_limiter_.RecordFailure(req->uid);
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{user::kInvalidCredentials.code, user::kInvalidCredentials.msg});
        conn->Close();
        return;
    }

    auto& user = *user_opt;

    // 4. 检查用户状态
    if (user.status == 2) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{user::kUserBanned.code, user::kUserBanned.msg});
        conn->Close();
        return;
    }

    // 5. 验证密码
    bool ok = PasswordUtils::Verify(req->password, user.password_hash);

    // 安全：立即清除内存中的明文密码
    if (!req->password.empty()) {
        volatile char* p = req->password.data();
        std::memset(const_cast<char*>(static_cast<const volatile char*>(p)), 0, req->password.size());
        req->password.clear();
    }

    if (!ok) {
        login_limiter_.RecordFailure(req->uid);
        // 与"用户不存在"使用相同的错误码和消息，防止用户枚举
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{user::kInvalidCredentials.code, user::kInvalidCredentials.msg});
        conn->Close();
        return;
    }

    // 登录成功，重置频率限制计数
    login_limiter_.Reset(req->uid);

    // 6. 设置连接状态
    conn->set_user_id(user.id);
    if (!req->device_id.empty()) {
        conn->set_device_id(req->device_id);
    }

    // 7. 注册到连接管理器（ConnManager 自动维护在线计数）
    ctx_.conn_manager().Add(user.id, conn);

    NOVA_NLOG_INFO(kLogTag, "user {} (id={}) logged in, device={}",
                   req->uid, user.id, req->device_id);

    // 8. 返回 LoginAck
    SendPacket(conn, Cmd::kLoginAck, seq, static_cast<uint64_t>(user.id),
               proto::LoginAck{kOk.code, kOk.msg, user.id, user.nickname, user.avatar});
}

void UserService::HandleLogout(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id != 0) {
        ctx_.conn_manager().Remove(user_id, conn.get());
        conn->set_user_id(0);  // 清零防止 Gateway 断连回调再次操作
        NOVA_NLOG_INFO(kLogTag, "user id={} logged out", user_id);
    }

    SendPacket(conn, Cmd::kLogout, pkt.seq, static_cast<uint64_t>(user_id),
               proto::RspBase{kOk.code, "goodbye"});
    conn->Close();
}

void UserService::HandleHeartbeat(ConnectionPtr conn, Packet& pkt) {
    SendPacket(conn, Cmd::kHeartbeatAck, pkt.seq,
               static_cast<uint64_t>(conn->user_id()), proto::RspBase{kOk.code, {}});
}

} // namespace nova
