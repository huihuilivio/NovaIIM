#include "user_service.h"
#include "errors/user_errors.h"
#include "../core/logger.h"
#include "../admin/password_utils.h"
#include "../dao/user_dao.h"

#include <algorithm>
#include <cctype>

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "UserService";

// uid 合法性校验：3-32 字符，仅允许 a-z 0-9 _ -
static bool IsValidUid(const std::string& uid) {
    if (uid.size() < 3 || uid.size() > 32)
        return false;
    return std::all_of(uid.begin(), uid.end(), [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; });
}

void UserService::HandleRegister(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    // 1. 反序列化 body → RegisterReq
    auto req = proto::Deserialize<proto::RegisterReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0, proto::RegisterAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    // 2. 校验 uid
    if (req->uid.empty()) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kUidRequired.code, ec::user::kUidRequired.msg});
        return;
    }
    if (req->uid.size() < 3) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kUidTooShort.code, ec::user::kUidTooShort.msg});
        return;
    }
    if (req->uid.size() > 32) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kUidTooLong.code, ec::user::kUidTooLong.msg});
        return;
    }
    if (!IsValidUid(req->uid)) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kUidInvalidChars.code, ec::user::kUidInvalidChars.msg});
        return;
    }

    // 3. 校验 password
    if (req->password.empty() || req->password.size() < 6) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kPasswordTooShort.code, ec::user::kPasswordTooShort.msg});
        return;
    }
    if (req->password.size() > 128) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kPasswordTooLong.code, ec::user::kPasswordTooLong.msg});
        return;
    }

    // 4. 检查 uid 是否已存在
    if (ctx_.dao().User().FindByUid(req->uid)) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kUidAlreadyExists.code, ec::user::kUidAlreadyExists.msg});
        return;
    }

    // 5. 哈希密码
    auto hash = PasswordUtils::Hash(req->password);

    // 安全：清除明文密码
    if (!req->password.empty()) {
        volatile char* p = reinterpret_cast<volatile char*>(req->password.data());
        for (size_t i = 0; i < req->password.size(); ++i)
            p[i] = 0;
        req->password.clear();
    }

    if (hash.empty()) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kRegisterFailed.code, ec::user::kRegisterFailed.msg});
        return;
    }

    // 6. 创建用户
    User user;
    user.uid           = req->uid;
    user.password_hash = std::move(hash);
    user.nickname      = req->nickname.empty() ? req->uid : req->nickname;
    user.status        = 1;

    if (!ctx_.dao().User().Insert(user)) {
        // Insert 失败可能是并发 uid 竞争
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kUidAlreadyExists.code, ec::user::kUidAlreadyExists.msg});
        return;
    }

    NOVA_NLOG_INFO(kLogTag, "user registered: uid={}, id={}", req->uid, user.id);

    SendPacket(conn, Cmd::kRegisterAck, seq, 0, proto::RegisterAck{ec::kOk.code, ec::kOk.msg, user.id});
}

void UserService::HandleLogin(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    // 连接已认证的情况下重新登录：先清理旧会话
    if (conn->is_authenticated()) {
        ctx_.conn_manager().Remove(conn->user_id(), conn.get());
        conn->set_user_id(0);
    }

    // 1. 反序列化 body → LoginReq
    auto req = proto::Deserialize<proto::LoginReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        conn->Close();
        return;
    }

    if (req->uid.empty()) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kUidRequired.code, ec::user::kUidRequired.msg});
        conn->Close();
        return;
    }

    if (req->password.empty()) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kPasswordRequired.code, ec::user::kPasswordRequired.msg});
        conn->Close();
        return;
    }

    // 2. 频率限制检查（防暴力破解）
    if (!login_limiter_.Allow(req->uid)) {
        NOVA_NLOG_WARN(kLogTag, "login rate limited for uid={}", req->uid);
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kRateLimited.code, ec::user::kRateLimited.msg});
        conn->Close();
        return;
    }

    // 3. 查询用户
    auto user_opt = ctx_.dao().User().FindByUid(req->uid);
    if (!user_opt) {
        // 统一错误信息，防止用户枚举
        login_limiter_.RecordFailure(req->uid);
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kInvalidCredentials.code, ec::user::kInvalidCredentials.msg});
        conn->Close();
        return;
    }

    auto& user = *user_opt;

    // 4. 检查用户状态
    if (user.status == 2) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kUserBanned.code, ec::user::kUserBanned.msg});
        conn->Close();
        return;
    }

    // 5. 验证密码
    bool ok = PasswordUtils::Verify(req->password, user.password_hash);

    // 安全：立即清除内存中的明文密码
    // 使用逐字节 volatile 写入，保证不被编译器优化掉
    // （memset + volatile cast 可能被优化，因 memset 参数本身不是 volatile）
    if (!req->password.empty()) {
        volatile char* p = reinterpret_cast<volatile char*>(req->password.data());
        for (size_t i = 0; i < req->password.size(); ++i)
            p[i] = 0;
        req->password.clear();
    }

    if (!ok) {
        login_limiter_.RecordFailure(req->uid);
        // 与"用户不存在"使用相同的错误码和消息，防止用户枚举
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kInvalidCredentials.code, ec::user::kInvalidCredentials.msg});
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

    NOVA_NLOG_INFO(kLogTag, "user {} (id={}) logged in, device={}", req->uid, user.id, req->device_id);

    // 8. 返回 LoginAck
    SendPacket(conn, Cmd::kLoginAck, seq, static_cast<uint64_t>(user.id),
               proto::LoginAck{ec::kOk.code, ec::kOk.msg, user.id, user.nickname, user.avatar});
}

void UserService::HandleLogout(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id != 0) {
        ctx_.conn_manager().Remove(user_id, conn.get());
        conn->set_user_id(0);  // 清零防止 Gateway 断连回调再次操作
        NOVA_NLOG_INFO(kLogTag, "user id={} logged out", user_id);
    }

    SendPacket(conn, Cmd::kLogout, pkt.seq, static_cast<uint64_t>(user_id), proto::RspBase{ec::kOk.code, "goodbye"});
    conn->Close();
}

void UserService::HandleHeartbeat(ConnectionPtr conn, Packet& pkt) {
    SendPacket(conn, Cmd::kHeartbeatAck, pkt.seq, static_cast<uint64_t>(conn->user_id()),
               proto::RspBase{ec::kOk.code, {}});
}

}  // namespace nova
