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

    // 注册一个测试用户并返回 uid 和 user_id
    struct RegisterResult {
        std::string uid;
        int64_t user_id = 0;
    };
    RegisterResult RegisterUser(const std::string& nickname, const std::string& password) {
        auto conn = std::make_shared<MockConnection>();
        auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{nickname, password});
        svc_->HandleRegister(conn, pkt);
        auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
        if (ack && ack->code == 0) return {ack->uid, ack->user_id};
        return {};
    }

    // 登录并返回 conn + LoginAck
    struct LoginResult {
        std::shared_ptr<MockConnection> conn;
        proto::LoginAck ack;
    };
    LoginResult DoLogin(const std::string& uid, const std::string& password,
                        const std::string& device_id = "test-device",
                        const std::string& device_type = "pc") {
        auto conn = std::make_shared<MockConnection>();
        auto pkt  = MakePacket(Cmd::kLogin, proto::LoginReq{uid, password, device_id, device_type});
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
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"Alice", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_GT(ack->user_id, 0);
    EXPECT_FALSE(ack->uid.empty());  // 服务端生成的 UID
}

TEST_F(UserServiceTest, RegisterDuplicateNicknameAllowed) {
    RegisterUser("bob", "pass123");

    // 同昵称可以重复注册
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"bob", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);  // 昵称可重复，注册应成功
}

TEST_F(UserServiceTest, RegisterEmptyNickname) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"", "pass123"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

TEST_F(UserServiceTest, RegisterShortPassword) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{"charlie", "12345"});
    svc_->HandleRegister(conn, pkt);

    auto ack = proto::Deserialize<proto::RegisterAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_NE(ack->code, 0);
}

// ============================================================
//  登录
// ============================================================

TEST_F(UserServiceTest, LoginSuccess) {
    auto reg = RegisterUser("alice", "pass123");
    auto [conn, ack] = DoLogin(reg.uid, "pass123");

    EXPECT_EQ(ack.code, 0);
    EXPECT_GT(ack.user_id, 0);
    EXPECT_FALSE(ack.nickname.empty());
    EXPECT_FALSE(conn->closed);
    EXPECT_TRUE(conn->is_authenticated());
    EXPECT_EQ(conn->user_id(), ack.user_id);
}

TEST_F(UserServiceTest, LoginWrongPassword) {
    auto reg = RegisterUser("alice", "pass123");
    auto [conn, ack] = DoLogin(reg.uid, "wrongpass");

    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
    EXPECT_FALSE(conn->is_authenticated());
}

TEST_F(UserServiceTest, LoginUserNotFound) {
    auto [conn, ack] = DoLogin("nonexistent", "pass123");

    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

TEST_F(UserServiceTest, LoginEmptyUid) {
    auto [conn, ack] = DoLogin("", "pass123");

    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

TEST_F(UserServiceTest, LoginEmptyPassword) {
    auto reg = RegisterUser("alice", "pass123");
    auto [conn, ack] = DoLogin(reg.uid, "");

    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

TEST_F(UserServiceTest, LoginRateLimit) {
    auto reg = RegisterUser("alice", "pass123");

    // 3 次失败（= login_max_attempts）
    for (int i = 0; i < 3; ++i) {
        DoLogin(reg.uid, "wrongpass");
    }

    // 第 4 次即使密码正确也应被限流
    auto [conn, ack] = DoLogin(reg.uid, "pass123");
    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

TEST_F(UserServiceTest, LoginBannedUser) {
    auto reg = RegisterUser("alice", "pass123");
    auto session = ctx_->dao().Session();
    auto user    = ctx_->dao().User().FindByUid(reg.uid);
    ASSERT_TRUE(user.has_value());
    ctx_->dao().User().UpdateStatus(user->id, static_cast<int>(AccountStatus::Banned));  // 封禁

    auto [conn, ack] = DoLogin(reg.uid, "pass123");
    EXPECT_NE(ack.code, 0);
    EXPECT_TRUE(conn->closed);
}

// ============================================================
//  设备持久化
// ============================================================

TEST_F(UserServiceTest, LoginPersistsDevice) {
    auto reg = RegisterUser("alice", "pass123");
    auto [conn, ack] = DoLogin(reg.uid, "pass123", "pc-001", "pc");
    ASSERT_EQ(ack.code, 0);

    auto session = ctx_->dao().Session();
    auto devices = ctx_->dao().User().ListDevicesByUser(ack.user_id);
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0].device_id, "pc-001");
    EXPECT_EQ(devices[0].device_type, "pc");
    EXPECT_FALSE(devices[0].last_active_at.empty());
}

TEST_F(UserServiceTest, LoginUpdatesExistingDevice) {
    auto reg = RegisterUser("alice", "pass123");

    // 第一次登录
    DoLogin(reg.uid, "pass123", "phone-001", "mobile");
    // 第二次同设备登录（更新 device_type）
    DoLogin(reg.uid, "pass123", "phone-001", "tablet");

    auto session = ctx_->dao().Session();
    auto devices = ctx_->dao().User().ListDevicesByUser(
        ctx_->dao().User().FindByUid(reg.uid)->id);
    ASSERT_EQ(devices.size(), 1u);  // 同一 device_id 只有 1 条记录
    EXPECT_EQ(devices[0].device_type, "tablet");
}

TEST_F(UserServiceTest, LoginMultipleDevices) {
    auto reg = RegisterUser("alice", "pass123");
    DoLogin(reg.uid, "pass123", "pc-001", "pc");
    DoLogin(reg.uid, "pass123", "phone-001", "mobile");

    auto session = ctx_->dao().Session();
    auto devices = ctx_->dao().User().ListDevicesByUser(
        ctx_->dao().User().FindByUid(reg.uid)->id);
    EXPECT_EQ(devices.size(), 2u);
}

// ============================================================
//  多端踢下线
// ============================================================

TEST_F(UserServiceTest, SameDeviceKicksOldConnection) {
    auto reg = RegisterUser("alice", "pass123");

    auto [conn1, ack1] = DoLogin(reg.uid, "pass123", "pc-001", "pc");
    ASSERT_EQ(ack1.code, 0);
    EXPECT_FALSE(conn1->closed);

    // 同一 device_id 再次登录
    auto [conn2, ack2] = DoLogin(reg.uid, "pass123", "pc-001", "pc");
    ASSERT_EQ(ack2.code, 0);

    // 旧连接应被踢下线
    EXPECT_TRUE(conn1->closed);
    EXPECT_FALSE(conn2->closed);
}

TEST_F(UserServiceTest, DifferentDeviceKeepsBoth) {
    auto reg = RegisterUser("alice", "pass123");

    auto [conn1, ack1] = DoLogin(reg.uid, "pass123", "pc-001", "pc");
    auto [conn2, ack2] = DoLogin(reg.uid, "pass123", "phone-001", "mobile");

    EXPECT_FALSE(conn1->closed);
    EXPECT_FALSE(conn2->closed);

    auto conns = ctx_->conn_manager().GetConns(ack1.user_id);
    EXPECT_EQ(conns.size(), 2u);
}

// ============================================================
//  登出
// ============================================================

TEST_F(UserServiceTest, LogoutSuccess) {
    auto reg = RegisterUser("alice", "pass123");
    auto [conn, ack] = DoLogin(reg.uid, "pass123");
    ASSERT_EQ(ack.code, 0);

    Packet pkt;
    pkt.cmd = static_cast<uint16_t>(Cmd::kLogout);
    pkt.seq = 2;
    svc_->HandleLogout(conn, pkt);

    EXPECT_TRUE(conn->closed);
    EXPECT_FALSE(ctx_->conn_manager().IsOnline(ack.user_id));
}

// ============================================================
//  心跳
// ============================================================

TEST_F(UserServiceTest, HeartbeatAck) {
    auto reg = RegisterUser("alice", "pass123");
    auto [conn, ack] = DoLogin(reg.uid, "pass123");
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
    auto reg_alice = RegisterUser("alice", "pass123");
    auto reg_bob   = RegisterUser("bob", "pass456");

    auto conn = std::make_shared<MockConnection>();

    // 第一次登录 alice
    auto pkt1 = MakePacket(Cmd::kLogin, proto::LoginReq{reg_alice.uid, "pass123", "dev1", "pc"});
    svc_->HandleLogin(conn, pkt1);
    auto ack1 = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack1 && ack1->code == 0);

    // 同连接再登录 bob（应先清理 alice 会话）
    auto pkt2 = MakePacket(Cmd::kLogin, proto::LoginReq{reg_bob.uid, "pass456", "dev1", "pc"});
    svc_->HandleLogin(conn, pkt2);
    auto ack2 = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
    ASSERT_TRUE(ack2 && ack2->code == 0);
    EXPECT_EQ(conn->user_id(), ack2->user_id);

    // alice 不应再在线（该连接已切换为 bob）
    EXPECT_FALSE(ctx_->conn_manager().IsOnline(ack1->user_id));
    EXPECT_TRUE(ctx_->conn_manager().IsOnline(ack2->user_id));
}

}  // namespace
}  // namespace nova
