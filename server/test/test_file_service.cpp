// test_file_service.cpp — FileService 单元测试
// 使用内存 SQLite + MockConnection 测试文件元数据管理

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "core/app_config.h"
#include "core/server_context.h"
#include "dao/dao_factory.h"
#include <nova/packet.h>
#include <nova/protocol.h>
#include <nova/errors.h>
#include "net/connection.h"
#include "service/user_service.h"
#include "service/file_service.h"

namespace nova {
namespace {

// ---- MockConnection ----
class MockConnection : public Connection {
public:
    void Send(const Packet& pkt) override {
        last_pkt = pkt;
        ++send_count;
    }
    void SendEncoded(const std::string& data) override { ++push_count; }
    void Close() override { closed = true; }

    Packet last_pkt;
    int send_count = 0;
    int push_count = 0;
    bool closed    = false;
};

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
class FileServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        DatabaseConfig db_cfg;
        db_cfg.type = "sqlite";
        db_cfg.path = ":memory:";

        ctx_ = std::make_unique<ServerContext>(cfg_);
        ctx_->set_dao(CreateDaoFactory(db_cfg));
        user_svc_ = std::make_unique<UserService>(*ctx_);
        file_svc_ = std::make_unique<FileService>(*ctx_);
    }

    struct UserInfo {
        std::shared_ptr<MockConnection> conn;
        std::string uid;
        int64_t user_id = 0;
    };

    UserInfo CreateAndLogin(const std::string& email, const std::string& nickname) {
        auto reg_conn = std::make_shared<MockConnection>();
        auto reg_pkt  = MakePacket(Cmd::kRegister, proto::RegisterReq{email, nickname, "password123"});
        user_svc_->HandleRegister(reg_conn, reg_pkt);

        auto conn      = std::make_shared<MockConnection>();
        auto login_pkt = MakePacket(Cmd::kLogin, proto::LoginReq{email, "password123", "dev1", "pc"});
        user_svc_->HandleLogin(conn, login_pkt);
        auto login_ack = proto::Deserialize<proto::LoginAck>(conn->last_pkt.body);
        EXPECT_TRUE(login_ack.has_value());
        EXPECT_EQ(login_ack->code, 0);
        return {conn, login_ack->uid, conn->user_id()};
    }

    AppConfig cfg_;
    std::unique_ptr<ServerContext> ctx_;
    std::unique_ptr<UserService> user_svc_;
    std::unique_ptr<FileService> file_svc_;
};

// ================================================================
//  Upload
// ================================================================

TEST_F(FileServiceTest, UploadSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadReq,
                          proto::UploadReq{"photo.png", 1024, "image/png", "", "image"});
    file_svc_->HandleUpload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->code, 0);
    EXPECT_GT(ack->file_id, 0);
    EXPECT_FALSE(ack->upload_url.empty());
    EXPECT_EQ(ack->already_exists, 0);
}

TEST_F(FileServiceTest, UploadFileNameRequired) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadReq,
                          proto::UploadReq{"", 1024, "image/png", "", "image"});
    file_svc_->HandleUpload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kFileNameRequired.code);
}

TEST_F(FileServiceTest, UploadFileSizeInvalid) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadReq,
                          proto::UploadReq{"f.txt", 0, "text/plain", "", "file"});
    file_svc_->HandleUpload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kFileSizeInvalid.code);
}

TEST_F(FileServiceTest, UploadFileTooLarge) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadReq,
                          proto::UploadReq{"f.bin", 200LL * 1024 * 1024, "application/octet-stream", "", "file"});
    file_svc_->HandleUpload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kFileSizeTooLarge.code);
}

TEST_F(FileServiceTest, UploadMimeTypeRequired) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadReq,
                          proto::UploadReq{"f.txt", 100, "", "", "file"});
    file_svc_->HandleUpload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kMimeTypeRequired.code);
}

TEST_F(FileServiceTest, UploadNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kUploadReq,
                           proto::UploadReq{"f.txt", 100, "text/plain", "", "file"});
    file_svc_->HandleUpload(conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::kNotAuthenticated.code);
}

// ================================================================
//  UploadComplete
// ================================================================

TEST_F(FileServiceTest, UploadCompleteSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    // 先上传
    auto up_pkt = MakePacket(Cmd::kUploadReq,
                             proto::UploadReq{"photo.png", 1024, "image/png", "", "image"});
    file_svc_->HandleUpload(alice.conn, up_pkt);
    auto up_ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    ASSERT_EQ(up_ack->code, 0);

    auto pkt = MakePacket(Cmd::kUploadComplete,
                          proto::UploadCompleteReq{up_ack->file_id});
    file_svc_->HandleUploadComplete(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadCompleteAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);
    EXPECT_FALSE(ack->file_path.empty());
}

TEST_F(FileServiceTest, UploadCompleteNotFound) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadComplete,
                          proto::UploadCompleteReq{99999});
    file_svc_->HandleUploadComplete(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadCompleteAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kFileNotFound.code);
}

// ================================================================
//  Download
// ================================================================

TEST_F(FileServiceTest, DownloadSuccess) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    // 先上传
    auto up_pkt = MakePacket(Cmd::kUploadReq,
                             proto::UploadReq{"doc.pdf", 2048, "application/pdf", "", "file"});
    file_svc_->HandleUpload(alice.conn, up_pkt);
    auto up_ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    ASSERT_EQ(up_ack->code, 0);

    auto pkt = MakePacket(Cmd::kDownloadReq, proto::DownloadReq{up_ack->file_id, 0});
    file_svc_->HandleDownload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::DownloadAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, 0);
    EXPECT_EQ(ack->file_name, "doc.pdf");
    EXPECT_EQ(ack->file_size, 2048);
    EXPECT_FALSE(ack->download_url.empty());
}

TEST_F(FileServiceTest, DownloadNotFound) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kDownloadReq, proto::DownloadReq{99999, 0});
    file_svc_->HandleDownload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::DownloadAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kFileNotFound.code);
}

TEST_F(FileServiceTest, DownloadNotAuth) {
    auto conn = std::make_shared<MockConnection>();
    auto pkt  = MakePacket(Cmd::kDownloadReq, proto::DownloadReq{1, 0});
    file_svc_->HandleDownload(conn, pkt);
    auto ack = proto::Deserialize<proto::DownloadAckMsg>(conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::kNotAuthenticated.code);
}

TEST_F(FileServiceTest, DownloadOtherUserFile) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");
    auto bob   = CreateAndLogin("bob@test.com", "Bob");

    // Alice uploads
    auto up_pkt = MakePacket(Cmd::kUploadReq,
                             proto::UploadReq{"secret.pdf", 1024, "application/pdf", "", "file"});
    file_svc_->HandleUpload(alice.conn, up_pkt);
    auto up_ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    ASSERT_EQ(up_ack->code, 0);

    // Bob tries to download Alice's file
    auto pkt = MakePacket(Cmd::kDownloadReq, proto::DownloadReq{up_ack->file_id, 0});
    file_svc_->HandleDownload(bob.conn, pkt);
    auto ack = proto::Deserialize<proto::DownloadAckMsg>(bob.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kFileNotFound.code);
}

// ================================================================
//  Path traversal prevention
// ================================================================

TEST_F(FileServiceTest, UploadPathTraversal) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadReq,
                          proto::UploadReq{"../../etc/passwd", 100, "text/plain", "", "file"});
    file_svc_->HandleUpload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    ASSERT_EQ(ack->code, 0);
    // The path should only contain the filename, not the traversal
    EXPECT_TRUE(ack->upload_url.find("..") == std::string::npos);
    EXPECT_TRUE(ack->upload_url.find("passwd") != std::string::npos);
}

TEST_F(FileServiceTest, UploadDotDotFileName) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadReq,
                          proto::UploadReq{"..", 100, "text/plain", "", "file"});
    file_svc_->HandleUpload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kFileNameRequired.code);
}

TEST_F(FileServiceTest, UploadInvalidFileType) {
    auto alice = CreateAndLogin("alice@test.com", "Alice");

    auto pkt = MakePacket(Cmd::kUploadReq,
                          proto::UploadReq{"f.txt", 100, "text/plain", "", "'; DROP TABLE user_files;--"});
    file_svc_->HandleUpload(alice.conn, pkt);
    auto ack = proto::Deserialize<proto::UploadAckMsg>(alice.conn->last_pkt.body);
    EXPECT_EQ(ack->code, errc::file::kInvalidFileType.code);
}

}  // namespace
}  // namespace nova
