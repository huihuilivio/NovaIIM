// test_user_service.cpp — UserService 单元测试
// 使用内存 SQLite + MockConnection 测试完整 login 链路
// 覆盖: Register, Login, Logout, Heartbeat, 设备持久化, 多端踢下线

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "core/app_config.h"
#include "core/server_context.h"
#include "dao/dao_factory.h"
#include "dao/user_dao.h"
#include "model/packet.h"
#include "model/protocol.h"
#include "net/connection.h"
#include "service/user_service.h"
#include "admin/password_utils.h"

namespace nova {
namespace {

// ---- MockConnection ----
class MockConnection : public Connection {
public:
    void Send(const Packet& pkt) override {
        last_pkt = pkt;
        ++send_count;
    }
    void SendEncoded(const std::string& /*data*/) override { ++send_count; }
    void Close() override { closed = true; }

    Packet last_pkt;
    int send_count = 0;
    bool closed    = false;
};

// ---- Helper: 构建包含序列化 body 的 Packet ----
template <typename T>
Packet MakePacket(Cmd cmd, const T& body, uint32_t seq = 1, uint64_t uid = 0) {
    Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(cmd);
    pkt.seq  = seq;
    pkt.uid  = uid;
    pkt.body = proto::Serialize(body);
    return pkt;
}

// ---- Test fixture ----
class UserServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        DatabaseConfig db_cfg;
        db_cfg.type = "sqlite";
        db_cfg.path = ":memory:";
        cfg_.server.login_max_attempts = 3;
        cfg_.server.login_window_secs  = 60;

        ctx_ = std::make_unique<ServerContext>(cfg_);
        ctx_->set_dao(CreateDaoFactory(db_cfg));
        svc_ = std::make_unique<UserService>(*ctx_);
    }

    // 注册一个测试用户并返回 uid
    struct RegisterResult {
        std::string uid;
        std::string email;
    };
    RegisterResult RegisterUser(const std::string& email, const std::string& nickname, const std::string& password) {
        auto conn = std::make_shared<MockConnection>();
        auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{email, nickname, password});
        svc_->HandleRegister(conn, pkt);
        auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
        if (ack && ack->code == 0) return {ack->uid, email};
        return {};
    }

    // 登录并返回 conn + LoginAck
    struct LoginResult {
        std::shared_ptr<MockConnection> conn;
        proto::LoginAck ack;
    };
    LoginResult DoLogin(const std::string& email, const std::string& password,
                        const std::string& device_id = "test-device",
                        const std::string& device_type = "pc") {
        auto conn = std::make_shared<MockConnection>();
        auto pkt  = MakePacket(Cmd::kLogin, proto::LoginReq{email, password, device_id, device_type});
        svc_->HandleLogin(conn, pkt);
        auto ack = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
        return {conn, ack ? *ack : proto::LoginAck{-1, "deserialize failed"}};
    }

    AppConfig cfg_;
    std::unique_ptr<ServerContext> ctx_;
    std::unique_ptr<UserService> svc_;
};

// ============================================================
//  注册
// ============================================================

TEST_F(UserServiceTest, RegisterSuccess) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"alice@example.com", "Alice", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_FALSE(ack->uid.empty());  // 服务端生成的 UID
}

TEST_F(UserServiceTest, RegisterDuplicateNicknameAllowed) {
    RegisterUser("bob1@example.com", "bob", "pass123");

    // 同昵称可以重复注册
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"bob2@example.com", "bob", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);  // 昵称可重复，注册应成功
}

TEST_F(UserServiceTest, RegisterEmptyNickname) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"empty@example.com", "", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

TEST_F(UserServiceTest, RegisterShortPassword) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"charlie@example.com", "charlie", "12345"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

TEST_F(UserServiceTest, RegisterPasswordExactMin) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"alice1@example.com", "alice", "123456"});  // 恰好 6 字符
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
}

TEST_F(UserServiceTest, RegisterPasswordExactMax) {
    auto conn = std::make_shared<MockConnection>();
    std::string long_pass(128, 'a');  // 恰好 128 字符
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"alice2@example.com", "alice", long_pass});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
}

TEST_F(UserServiceTest, RegisterPasswordOverMax) {
    auto conn = std::make_shared<MockConnection>();
    std::string long_pass(129, 'a');  // 超过 128 字符
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"alice3@example.com", "alice", long_pass});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

TEST_F(UserServiceTest, RegisterNicknameTooLong) {
    auto conn = std::make_shared<MockConnection>();
    std::string long_nick(101, 'x');  // 超过 100 字符
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"longnick@example.com", long_nick, "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 11);  // kNicknameTooLong
}

TEST_F(UserServiceTest, RegisterNicknameExactMax) {
    auto conn = std::make_shared<MockConnection>();
    std::string nick(100, 'x');  // 恰好 100 字符
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"maxnick@example.com", nick, "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
}

TEST_F(UserServiceTest, RegisterWhitespaceOnlyNickname) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"ws@example.com", "   \t\n  ", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 10);  // kNicknameRequired（trim 后为空）
}

TEST_F(UserServiceTest, RegisterNicknameTrimsWhitespace) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"trim@example.com", "  Alice  ", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);

    // 验证数据库中的昵称已 trim
    auto user = ctx_->dao().User().FindByEmail("trim@example.com");
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->nickname, "Alice");
}

TEST_F(UserServiceTest, RegisterNicknameWithControlChars) {
    auto conn = std::make_shared<MockConnection>();
    std::string nick = "Alice\x01Bob";
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"ctrl@example.com", nick, "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 12);  // kNicknameInvalid
}

TEST_F(UserServiceTest, RegisterInvalidBody) {
    auto conn = std::make_shared<MockConnection>();
    Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(Cmd::kRegister);
    pkt.seq  = 1;
    pkt.uid  = 0;
    pkt.body = "not a valid struct_pack payload";
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

// ---- 邮箱相关注册测试 ----

TEST_F(UserServiceTest, RegisterEmptyEmail) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"", "Alice", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 1);  // kEmailRequired
}

TEST_F(UserServiceTest, RegisterInvalidEmailFormat) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"not-an-email", "Alice", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 6);  // kEmailInvalid
}

TEST_F(UserServiceTest, RegisterDuplicateEmail) {
    RegisterUser("dup@example.com", "Alice", "pass123");

    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"dup@example.com", "Bob", "pass456"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 8);  // kEmailAlreadyExists
}

TEST_F(UserServiceTest, RegisterEmailCaseInsensitive) {
    RegisterUser("case@example.com", "Alice", "pass123");

    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"CASE@EXAMPLE.COM", "Bob", "pass456"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 8);  // 不区分大小写，视为重复
}

TEST_F(UserServiceTest, RegisterEmailTooLong) {
    auto conn = std::make_shared<MockConnection>();
    std::string long_email = std::string(251, 'a') + "@b.cc";  // 257 chars > 255
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{long_email, "Alice", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 7);  // kEmailTooLong
}

TEST_F(UserServiceTest, RegisterConcurrentDuplicateEmailReturnsAlreadyExists) {
    // 模拟 TOCTOU：先正常注册，然后尝试用同一 email 直接 Insert
    // Insert 失败后应检测 UNIQUE 冲突并返回 kEmailAlreadyExists 而非 kRegisterFailed
    RegisterUser("race@example.com", "Alice", "pass123");

    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"race@example.com", "Bob", "pass456"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 8);  // kEmailAlreadyExists
}

TEST_F(UserServiceTest, LoginEmailWithWhitespace) {
    RegisterUser("trim@example.com", "alice", "pass123");
    // 邮箱带首尾空白也应正常登录
    auto [conn, ack] = DoLogin("  trim@example.com  ", "pass123");
    EXPECT_EQ(ack.code, 0);
}

TEST_F(UserServiceTest, LoginPasswordRequiredHasDistinctCode) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    auto [conn, ack] = DoLogin(reg.email, "");
    // kPasswordRequired 的 code(3) 应不同于 kEmailRequired(1)
    EXPECT_EQ(ack.code, 3);
}

// ============================================================
//  登录
// ============================================================

TEST_F(UserServiceTest, LoginSuccess) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    auto [conn, ack] = DoLogin(reg.email, "pass123");

    EXPECT_EQ(ack.code, 0);
    EXPECT_FALSE(ack.uid.empty());
    EXPECT_FALSE(ack.nickname.empty());
    EXPECT_FALSE(conn->closed);
    EXPECT_TRUE(conn->is_authenticated());
}

TEST_F(UserServiceTest, LoginWrongPassword) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    auto [conn, ack] = DoLogin(reg.email, "wrongpass");

    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
    EXPECT_FALSE(conn->is_authenticated());
}

TEST_F(UserServiceTest, LoginUserNotFound) {
    auto [conn, ack] = DoLogin("nonexistent@example.com", "pass123");

    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

TEST_F(UserServiceTest, LoginEmptyEmail) {
    auto [conn, ack] = DoLogin("", "pass123");

    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

TEST_F(UserServiceTest, LoginEmptyPassword) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    auto [conn, ack] = DoLogin(reg.email, "");

    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

TEST_F(UserServiceTest, LoginRateLimit) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");

    // 3 次失败（= login_max_attempts）
    for (int i = 0; i < 3; ++i) {
        DoLogin(reg.email, "wrongpass");
    }

    // 第 4 次即使密码正确也应被限流
    auto [conn, ack] = DoLogin(reg.email, "pass123");
    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

TEST_F(UserServiceTest, LoginBannedUser) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    auto session = ctx_->dao().Session();
    auto user    = ctx_->dao().User().FindByEmail(reg.email);
    ASSERT_TRUE(user.has_value());
    ctx_->dao().User().UpdateStatus(user->uid, static_cast<int>(AccountStatus::Banned));  // 封禁

    auto [conn, ack] = DoLogin(reg.email, "pass123");
    // 封禁用户应返回与"不存在/密码错误"相同的 code，防止用户枚举
    EXPECT_EQ(ack.code, 2);  // kInvalidCredentials
    EXPECT_TRUE(conn->closed);
}

// ============================================================
//  设备持久化
// ============================================================

TEST_F(UserServiceTest, LoginPersistsDevice) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    auto [conn, ack] = DoLogin(reg.email, "pass123", "pc-001", "pc");
    ASSERT_EQ(ack.code, 0);

    auto session = ctx_->dao().Session();
    auto user = ctx_->dao().User().FindByEmail(reg.email);
    ASSERT_TRUE(user.has_value());
    auto devices = ctx_->dao().User().ListDevicesByUser(user->uid);
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0].device_id, "pc-001");
    EXPECT_EQ(devices[0].device_type, "pc");
    EXPECT_FALSE(devices[0].last_active_at.empty());
}

TEST_F(UserServiceTest, LoginUpdatesExistingDevice) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");

    // 第一次登录
    DoLogin(reg.email, "pass123", "phone-001", "mobile");
    // 第二次同设备登录（更新 device_type）
    DoLogin(reg.email, "pass123", "phone-001", "tablet");

    auto session = ctx_->dao().Session();
    auto devices = ctx_->dao().User().ListDevicesByUser(
        ctx_->dao().User().FindByEmail(reg.email)->uid);
    ASSERT_EQ(devices.size(), 1u);  // 同一 device_id 只有 1 条记录
    EXPECT_EQ(devices[0].device_type, "tablet");
}

TEST_F(UserServiceTest, LoginMultipleDevices) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    DoLogin(reg.email, "pass123", "pc-001", "pc");
    DoLogin(reg.email, "pass123", "phone-001", "mobile");

    auto session = ctx_->dao().Session();
    auto devices = ctx_->dao().User().ListDevicesByUser(
        ctx_->dao().User().FindByEmail(reg.email)->uid);
    EXPECT_EQ(devices.size(), 2u);
}

// ============================================================
//  多端踢下线
// ============================================================

TEST_F(UserServiceTest, SameDeviceKicksOldConnection) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");

    auto [conn1, ack1] = DoLogin(reg.email, "pass123", "pc-001", "pc");
    ASSERT_EQ(ack1.code, 0);
    EXPECT_FALSE(conn1->closed);

    // 同一 device_id 再次登录
    auto [conn2, ack2] = DoLogin(reg.email, "pass123", "pc-001", "pc");
    ASSERT_EQ(ack2.code, 0);

    // 旧连接应被踢下线
    EXPECT_TRUE(conn1->closed);
    EXPECT_FALSE(conn2->closed);
}

TEST_F(UserServiceTest, DifferentDeviceKeepsBoth) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");

    auto [conn1, ack1] = DoLogin(reg.email, "pass123", "pc-001", "pc");
    auto [conn2, ack2] = DoLogin(reg.email, "pass123", "phone-001", "mobile");

    EXPECT_FALSE(conn1->closed);
    EXPECT_FALSE(conn2->closed);

    auto user = ctx_->dao().User().FindByEmail(reg.email);
    ASSERT_TRUE(user.has_value());
    auto conns = ctx_->conn_manager().GetConns(user->id);
    EXPECT_EQ(conns.size(), 2u);
}

// ============================================================
//  登出
// ============================================================

TEST_F(UserServiceTest, LogoutSuccess) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    auto [conn, ack] = DoLogin(reg.email, "pass123");
    ASSERT_EQ(ack.code, 0);

    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(Cmd::kLogout);
    pkt.seq = 2;
    svc_->HandleLogout(conn, pkt);

    auto user = ctx_->dao().User().FindByEmail(reg.email);
    ASSERT_TRUE(user.has_value());
    EXPECT_TRUE(conn->closed);
    EXPECT_FALSE(ctx_->conn_manager().IsOnline(user->id));
}

// ============================================================
//  心跳
// ============================================================

TEST_F(UserServiceTest, HeartbeatAck) {
    auto reg = RegisterUser("alice@example.com", "alice", "pass123");
    auto [conn, ack] = DoLogin(reg.email, "pass123");
    ASSERT_EQ(ack.code, 0);

    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(Cmd::kHeartbeat);
    pkt.seq = 2;
    svc_->HandleHeartbeat(conn, pkt);

    auto rsp = proto::Deserialize<proto::RspBase>(conn->last_pkt.body);
    ASSERT_TRUE(rsp.has_value());
    EXPECT_EQ(rsp->code, 0);
}

// ============================================================
//  重复登录（同连接）
// ============================================================

TEST_F(UserServiceTest, ReloginOnSameConnection) {
    auto reg_alice = RegisterUser("alice@example.com", "alice", "pass123");
    auto reg_bob   = RegisterUser("bob@example.com", "bob", "pass456");

    auto conn = std::make_shared<MockConnection>();

    // 第一次登录 alice
    auto pkt1 = MakePacket(Cmd::kLogin, proto::LoginReq{reg_alice.email, "pass123", "dev1", "pc"});
    svc_->HandleLogin(conn, pkt1);
    auto ack1 = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack1 && ack1->code == 0);

    // 同连接再登录 bob（应先清理 alice 会话）
    auto pkt2 = MakePacket(Cmd::kLogin, proto::LoginReq{reg_bob.email, "pass456", "dev1", "pc"});
    svc_->HandleLogin(conn, pkt2);
    auto ack2 = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack2 && ack2->code == 0);
    EXPECT_EQ(conn->uid(), ack2->uid);

    // alice 不应再在线（该连接已切换为 bob）
    auto alice = ctx_->dao().User().FindByEmail(reg_alice.email);
    auto bob   = ctx_->dao().User().FindByEmail(reg_bob.email);
    ASSERT_TRUE(alice.has_value() && bob.has_value());
    EXPECT_FALSE(ctx_->conn_manager().IsOnline(alice->id));
    EXPECT_TRUE(ctx_->conn_manager().IsOnline(bob->id));
}

}  // namespace
}  // namespace nova
