#include "user_service.h"
#include "errors/user_errors.h"
#include "../core/logger.h"
#include "../admin/password_utils.h"
#include "../dao/user_dao.h"
#include "../dao/file_dao.h"

#include <algorithm>
#include <cctype>

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "UserService";

// Trim 首尾空白
static void TrimInPlace(std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) { s.clear(); return; }
    auto end = s.find_last_not_of(" \t\r\n");
    s = s.substr(start, end - start + 1);
}

// 检查是否包含控制字符（\x00-\x1F, \x7F）
static bool ContainsControlChars(const std::string& s) {
    return std::any_of(s.begin(), s.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7F;
    });
}

// 简单邮箱格式校验：non-empty@non-empty.non-empty，不含空白/控制字符
static bool IsValidEmail(const std::string& email) {
    auto at = email.find('@');
    if (at == std::string::npos || at == 0 || at == email.size() - 1)
        return false;
    auto dot = email.find('.', at + 1);
    if (dot == std::string::npos || dot == at + 1 || dot == email.size() - 1)
        return false;
    for (unsigned char c : email) {
        if (c <= 0x20 || c == 0x7F)
            return false;
    }
    return true;
}

// 将邮箱转为小写（不区分大小写）
static void EmailToLower(std::string& email) {
    std::transform(email.begin(), email.end(), email.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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

    // 2. 校验 email（必填，最长 255 字符，格式校验，不区分大小写）
    TrimInPlace(req->email);
    EmailToLower(req->email);
    if (req->email.empty()) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kEmailRequired.code, ec::user::kEmailRequired.msg});
        return;
    }
    if (req->email.size() > 255) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kEmailTooLong.code, ec::user::kEmailTooLong.msg});
        return;
    }
    if (!IsValidEmail(req->email)) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kEmailInvalid.code, ec::user::kEmailInvalid.msg});
        return;
    }

    // 3. 检查邮箱是否已注册
    if (ctx_.dao().User().FindByEmail(req->email)) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kEmailAlreadyExists.code, ec::user::kEmailAlreadyExists.msg});
        return;
    }

    // 4. 校验 nickname（必填，最长 100 字符，去除首尾空白，禁止控制字符）
    TrimInPlace(req->nickname);
    if (req->nickname.empty()) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kNicknameRequired.code, ec::user::kNicknameRequired.msg});
        return;
    }
    if (req->nickname.size() > 100) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kNicknameTooLong.code, ec::user::kNicknameTooLong.msg});
        return;
    }
    if (ContainsControlChars(req->nickname)) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kNicknameInvalid.code, ec::user::kNicknameInvalid.msg});
        return;
    }

    // 5. 校验 password
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

    // 6. 哈希密码
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

    // 7. 生成唯一 UID（Snowflake 算法，全局唯一无碰撞）
    User user;
    user.uid           = ctx_.snowflake().NextIdStr();
    user.email         = req->email;
    user.password_hash = hash;
    user.nickname      = req->nickname;
    user.status        = static_cast<int>(AccountStatus::Normal);

    if (ctx_.dao().User().Insert(user)) {
        NOVA_NLOG_INFO(kLogTag, "user registered: email={}, uid={}, nickname={}", user.email, user.uid, user.nickname);
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::kOk.code, ec::kOk.msg, user.uid});
        return;
    }

    // 插入失败 — 区分 UNIQUE 冲突（并发注册同一邮箱）与其他 DB 错误
    if (ctx_.dao().User().FindByEmail(req->email)) {
        SendPacket(conn, Cmd::kRegisterAck, seq, 0,
                   proto::RegisterAck{ec::user::kEmailAlreadyExists.code, ec::user::kEmailAlreadyExists.msg});
        return;
    }
    NOVA_NLOG_ERROR(kLogTag, "failed to insert user: email={}, uid={}, nickname={}", user.email, user.uid, user.nickname);
    SendPacket(conn, Cmd::kRegisterAck, seq, 0,
               proto::RegisterAck{ec::user::kRegisterFailed.code, ec::user::kRegisterFailed.msg});
}

void UserService::HandleLogin(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    // 连接已认证的情况下重新登录：先清理旧会话
    if (conn->is_authenticated()) {
        ctx_.conn_manager().Remove(conn->user_id(), conn.get());
        conn->set_user_id(0);
        conn->set_uid("");
    }

    // 1. 反序列化 body → LoginReq
    auto req = proto::Deserialize<proto::LoginReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0, proto::LoginAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        conn->Close();
        return;
    }

    if (req->email.empty()) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kEmailRequired.code, ec::user::kEmailRequired.msg});
        conn->Close();
        return;
    }

    if (req->password.empty()) {
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kPasswordRequired.code, ec::user::kPasswordRequired.msg});
        conn->Close();
        return;
    }

    // 邮箱不区分大小写，trim + lower
    TrimInPlace(req->email);
    EmailToLower(req->email);

    // 2. 频率限制检查（防暴力破解）
    if (!login_limiter_.Allow(req->email)) {
        NOVA_NLOG_WARN(kLogTag, "login rate limited for email={}", req->email);
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kRateLimited.code, ec::user::kRateLimited.msg});
        conn->Close();
        return;
    }

    // 3. 查询用户
    auto user_opt = ctx_.dao().User().FindByEmail(req->email);
    if (!user_opt) {
        // 统一错误信息，防止用户枚举
        login_limiter_.RecordFailure(req->email);
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kInvalidCredentials.code, ec::user::kInvalidCredentials.msg});
        conn->Close();
        return;
    }

    auto& user = *user_opt;

    // 4. 检查用户状态（封禁账户也返回 kInvalidCredentials，防止用户枚举）
    if (user.status == static_cast<int>(AccountStatus::Banned)) {
        login_limiter_.RecordFailure(req->email);
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kInvalidCredentials.code, ec::user::kInvalidCredentials.msg});
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
        login_limiter_.RecordFailure(req->email);
        // 与"用户不存在"使用相同的错误码和消息，防止用户枚举
        SendPacket(conn, Cmd::kLoginAck, seq, 0,
                   proto::LoginAck{ec::user::kInvalidCredentials.code, ec::user::kInvalidCredentials.msg});
        conn->Close();
        return;
    }

    // 登录成功，重置频率限制计数
    login_limiter_.Reset(req->email);

    // 6. 设置连接状态
    conn->set_user_id(user.id);
    conn->set_uid(user.uid);
    if (!req->device_id.empty()) {
        conn->set_device_id(req->device_id);
    }

    // 7. 注册到连接管理器（ConnManager 自动维护在线计数）
    ctx_.conn_manager().Add(user.id, conn);

    // 8. 持久化设备信息（upsert user_devices 表，供 Admin 查询）
    if (!req->device_id.empty()) {
        ctx_.dao().User().UpsertDevice(user.uid, req->device_id, req->device_type);
    }

    NOVA_NLOG_INFO(kLogTag, "user {} (uid={}) logged in, device={} type={}", req->email, user.uid, req->device_id,
                   req->device_type);

    // 9. 返回 LoginAck
    SendPacket(conn, Cmd::kLoginAck, seq, 0,
               proto::LoginAck{ec::kOk.code, ec::kOk.msg, user.uid, user.nickname, user.avatar});
}

void UserService::HandleLogout(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id != 0) {
        auto uid = conn->uid();
        ctx_.conn_manager().Remove(user_id, conn.get());
        conn->set_user_id(0);
        conn->set_uid("");
        conn->set_device_id("");
        NOVA_NLOG_INFO(kLogTag, "user id={} uid={} logged out", user_id, uid);
    }

    SendPacket(conn, Cmd::kLogout, pkt.seq, 0, proto::RspBase{ec::kOk.code, "goodbye"});
    conn->Close();
}

void UserService::HandleHeartbeat(ConnectionPtr conn, Packet& pkt) {
    int64_t user_id = conn->user_id();
    if (user_id == 0) {
        SendPacket(conn, Cmd::kHeartbeatAck, pkt.seq, 0,
                   proto::RspBase{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }
    SendPacket(conn, Cmd::kHeartbeatAck, pkt.seq, 0,
               proto::RspBase{ec::kOk.code, {}});
}

void UserService::HandleSearchUser(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    auto req = proto::Deserialize<proto::SearchUserReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kSearchUserAck, seq, 0,
                   proto::SearchUserAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    TrimInPlace(req->keyword);
    if (req->keyword.empty()) {
        SendPacket(conn, Cmd::kSearchUserAck, seq, 0,
                   proto::SearchUserAck{ec::user::kSearchKeywordEmpty.code, ec::user::kSearchKeywordEmpty.msg});
        return;
    }
    if (req->keyword.size() > 255) {
        SendPacket(conn, Cmd::kSearchUserAck, seq, 0,
                   proto::SearchUserAck{ec::user::kSearchKeywordTooLong.code, ec::user::kSearchKeywordTooLong.msg});
        return;
    }

    std::vector<User> results;
    if (req->keyword.find('@') != std::string::npos) {
        // 邮箱精确搜索
        std::string email = req->keyword;
        EmailToLower(email);
        auto user = ctx_.dao().User().FindByEmail(email);
        if (user) {
            results.push_back(std::move(*user));
        }
    } else {
        // 昵称模糊搜索
        results = ctx_.dao().User().SearchByNickname(req->keyword, 20);
    }

    // 脱敏：仅返回 uid / nickname / avatar
    proto::SearchUserAck ack;
    ack.code = ec::kOk.code;
    ack.msg  = ec::kOk.msg;
    ack.users.reserve(results.size());
    for (auto& u : results) {
        ack.users.push_back({std::move(u.uid), std::move(u.nickname), std::move(u.avatar)});
    }

    SendPacket(conn, Cmd::kSearchUserAck, seq, 0, ack);
}

void UserService::HandleGetProfile(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    auto req = proto::Deserialize<proto::GetUserProfileReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kGetUserProfileAck, seq, 0,
                   proto::GetUserProfileAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    bool is_self = req->target_uid.empty() || req->target_uid == conn->uid();
    std::optional<User> user;
    if (is_self) {
        user = ctx_.dao().User().FindByUid(conn->uid());
    } else {
        user = ctx_.dao().User().FindByUid(req->target_uid);
    }

    if (!user) {
        SendPacket(conn, Cmd::kGetUserProfileAck, seq, 0,
                   proto::GetUserProfileAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
        return;
    }

    proto::GetUserProfileAck ack;
    ack.code     = ec::kOk.code;
    ack.msg      = ec::kOk.msg;
    ack.uid      = user->uid;
    ack.nickname = user->nickname;
    ack.avatar   = user->avatar;
    if (is_self) {
        ack.email = user->email;  // 仅查自己时返回邮箱
    }

    SendPacket(conn, Cmd::kGetUserProfileAck, seq, 0, ack);
}

void UserService::HandleUpdateProfile(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;

    auto req = proto::Deserialize<proto::UpdateProfileReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
                   proto::UpdateProfileAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    TrimInPlace(req->nickname);
    TrimInPlace(req->avatar);

    if (req->nickname.empty() && req->avatar.empty()) {
        SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
                   proto::UpdateProfileAck{ec::user::kNothingToUpdate.code, ec::user::kNothingToUpdate.msg});
        return;
    }

    // 校验 nickname
    if (!req->nickname.empty()) {
        if (req->nickname.size() > 100) {
            SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
                       proto::UpdateProfileAck{ec::user::kNicknameTooLong.code, ec::user::kNicknameTooLong.msg});
            return;
        }
        if (ContainsControlChars(req->nickname)) {
            SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
                       proto::UpdateProfileAck{ec::user::kNicknameInvalid.code, ec::user::kNicknameInvalid.msg});
            return;
        }
    }

    // 校验 avatar
    if (!req->avatar.empty() && req->avatar.size() > 512) {
        SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
                   proto::UpdateProfileAck{ec::user::kAvatarPathTooLong.code, ec::user::kAvatarPathTooLong.msg});
        return;
    }

    std::string uid = conn->uid();

    // 先查询用户是否存在，避免半更新
    auto user = ctx_.dao().User().FindByUid(uid);
    if (!user) {
        SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
                   proto::UpdateProfileAck{ec::user::kUserNotFound.code, ec::user::kUserNotFound.msg});
        return;
    }

    // 同时更新 nickname 和 avatar，保证原子性
    if (!req->nickname.empty()) {
        if (!ctx_.dao().User().UpdateNickname(uid, req->nickname)) {
            SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
                       proto::UpdateProfileAck{ec::user::kUpdateProfileFailed.code, ec::user::kUpdateProfileFailed.msg});
            return;
        }
    }

    if (!req->avatar.empty()) {
        if (!ctx_.dao().User().UpdateAvatar(uid, req->avatar)) {
            // 回滚 nickname：恢复原值
            if (!req->nickname.empty()) {
                ctx_.dao().User().UpdateNickname(uid, user->nickname);
            }
            SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
                       proto::UpdateProfileAck{ec::user::kUpdateProfileFailed.code, ec::user::kUpdateProfileFailed.msg});
            return;
        }
        // 统一头像元数据管理
        ctx_.dao().File().SoftDeleteByUserAndType(user->id, "avatar");
        UserFile file;
        file.user_id   = user->id;
        file.file_type = "avatar";
        file.file_path = req->avatar;
        file.hash      = req->file_hash;
        ctx_.dao().File().Insert(file);
    }

    NOVA_NLOG_INFO(kLogTag, "user uid={} updated profile: nickname='{}', avatar='{}'", uid, req->nickname, req->avatar);
    SendPacket(conn, Cmd::kUpdateProfileAck, seq, 0,
               proto::UpdateProfileAck{ec::kOk.code, ec::kOk.msg});
}

}  // namespace nova
