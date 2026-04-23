// test_file_server.cpp — FileServer HTTP 集成测试
// 启动真实 FileServer (in-proc, 19092 端口) + 临时目录
// 使用 requests::get/post 同步 HTTP 客户端发起请求并验证响应
//
// 测试覆盖:
//   1. GET  /healthz                              — 健康检查
//   2. POST /api/v1/files/upload (raw body)       — 小文件上传
//   3. POST /api/v1/files/upload (multipart)      — multipart 上传
//   4. GET  /static/xxx                           — 静态文件下载/预览
//   5. DELETE /api/v1/files/{filename}            — 文件删除
//   6. 路径穿越防护                                — ".." / 绝对路径
//   7. POST /api/v1/files/upload/{filename}       — 大文件流式上传

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>
#include <memory>
#include <filesystem>
#include <fstream>

#include <hv/requests.h>
#include <hv/json.hpp>

#include "file/file_server.h"

namespace nova {
namespace {

namespace fs = std::filesystem;

static constexpr int kPort         = 19092;
static constexpr const char* kBase = "http://127.0.0.1:19092";

static std::string Url(const char* path) {
    return std::string(kBase) + path;
}

static nlohmann::json ParseJson(const requests::Response& resp) {
    EXPECT_NE(resp, nullptr);
    if (!resp)
        return {};
    return nlohmann::json::parse(resp->body, nullptr, false);
}

// ============================================================
// Test Fixture — 每个 Suite 共享一个 FileServer + 临时目录
// ============================================================

class FileServerTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        // 创建临时目录
        tmp_dir_ = (fs::temp_directory_path() / "nova_file_server_test").string();
        fs::create_directories(tmp_dir_);

        FileServer::Options opts;
        opts.port     = kPort;
        opts.root_dir = tmp_dir_;

        server_ = std::make_unique<FileServer>();
        int rc  = server_->Start(opts);
        ASSERT_EQ(rc, 0) << "FileServer failed to start on port " << kPort;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    static void TearDownTestSuite() {
        if (server_)
            server_->Stop();
        server_.reset();

        // 清理临时目录
        std::error_code ec;
        fs::remove_all(tmp_dir_, ec);
        fs::remove_all(fs::temp_directory_path() / "nova_upload_src", ec);
    }

    static std::string tmp_dir_;
    static std::unique_ptr<FileServer> server_;
};

std::string FileServerTest::tmp_dir_;
std::unique_ptr<FileServer> FileServerTest::server_;

// ============================================================
// 1. 健康检查
// ============================================================

TEST_F(FileServerTest, HealthzReturnsOk) {
    auto url  = Url("/healthz");
    auto resp = requests::get(url.c_str());
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);
    auto j = ParseJson(resp);
    EXPECT_EQ(j["status"], "ok");
}

// ============================================================
// 2. 小文件上传 (raw body)
// ============================================================

TEST_F(FileServerTest, UploadRawBodySuccess) {
    std::string body = "hello file server";
    auto url         = Url("/api/v1/files/upload?filename=test.txt");
    auto resp        = requests::post(url.c_str(), body);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    // 验证文件已创建
    std::string filepath = tmp_dir_ + "/test.txt";
    EXPECT_TRUE(fs::exists(filepath));

    // 读取验证内容
    std::ifstream ifs(filepath);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ(content, body);
}

TEST_F(FileServerTest, UploadRawBodyDefaultFilename) {
    std::string body = "default name test";
    auto url         = Url("/api/v1/files/upload");
    auto resp        = requests::post(url.c_str(), body);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    std::string filepath = tmp_dir_ + "/unnamed.txt";
    EXPECT_TRUE(fs::exists(filepath));
}

// ============================================================
// 3. 小文件上传 (multipart/form-data)
// ============================================================

TEST_F(FileServerTest, UploadMultipartSuccess) {
    // 在系统临时目录下创建独立的源目录，与服务器 root_dir 完全隔离
    std::string src_dir = (fs::temp_directory_path() / "nova_upload_src").string();
    fs::create_directories(src_dir);
    std::string src_file = src_dir + "/multipart_data.txt";
    std::string content  = "multipart upload content";
    {
        std::ofstream ofs(src_file);
        ofs << content;
    }

    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_POST;
    req->url    = Url("/api/v1/files/upload");
    req->SetFormFile("file", src_file.c_str());
    auto resp = requests::request(req);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    // SaveFormFile 使用 basename 保存到 root_dir
    std::string saved = tmp_dir_ + "/multipart_data.txt";
    ASSERT_TRUE(fs::exists(saved));

    // 验证内容一致
    std::ifstream ifs(saved);
    std::string actual((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
    EXPECT_EQ(actual, content);
}

// ============================================================
// 4. 静态文件下载/预览
// ============================================================

TEST_F(FileServerTest, StaticFileDownload) {
    // 先创建一个文件
    std::string filepath = tmp_dir_ + "/download_me.txt";
    {
        std::ofstream ofs(filepath);
        ofs << "download content";
    }

    auto url  = Url("/static/download_me.txt");
    auto resp = requests::get(url.c_str());
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);
    EXPECT_EQ(resp->body, "download content");
}

TEST_F(FileServerTest, StaticFileNotFound) {
    auto url  = Url("/static/nonexistent.txt");
    auto resp = requests::get(url.c_str());
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 404);
}

TEST_F(FileServerTest, UploadThenDownloadViaStatic) {
    // 上传
    std::string body = "roundtrip content";
    auto up_url      = Url("/api/v1/files/upload?filename=roundtrip.txt");
    auto up_resp     = requests::post(up_url.c_str(), body);
    ASSERT_NE(up_resp, nullptr);
    EXPECT_EQ(up_resp->status_code, 200);

    // 通过 /static 下载
    auto dl_url  = Url("/static/roundtrip.txt");
    auto dl_resp = requests::get(dl_url.c_str());
    ASSERT_NE(dl_resp, nullptr);
    EXPECT_EQ(dl_resp->status_code, 200);
    EXPECT_EQ(dl_resp->body, body);
}

// ============================================================
// 5. 文件删除
// ============================================================

TEST_F(FileServerTest, DeleteFileSuccess) {
    // 先创建文件
    std::string filepath = tmp_dir_ + "/to_delete.txt";
    {
        std::ofstream ofs(filepath);
        ofs << "will be deleted";
    }
    ASSERT_TRUE(fs::exists(filepath));

    auto url  = Url("/api/v1/files/to_delete.txt");
    auto resp = requests::Delete(url.c_str());
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);
    EXPECT_FALSE(fs::exists(filepath));
}

TEST_F(FileServerTest, DeleteFileNotFound) {
    auto url  = Url("/api/v1/files/no_such_file.txt");
    auto resp = requests::Delete(url.c_str());
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 404);
}

// ============================================================
// 6. 路径穿越防护
// ============================================================

TEST_F(FileServerTest, UploadPathTraversalRejected) {
    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_POST;
    req->url    = Url("/api/v1/files/upload");
    req->SetParam("filename", "../../etc/passwd");
    req->body         = "evil";
    req->content_type = TEXT_PLAIN;
    auto resp = requests::request(req);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 400);
}

TEST_F(FileServerTest, UploadAbsolutePathRejected) {
    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_POST;
    req->url    = Url("/api/v1/files/upload");
    req->SetParam("filename", "/tmp/evil.txt");
    req->body         = "evil";
    req->content_type = TEXT_PLAIN;
    auto resp = requests::request(req);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 400);
}

TEST_F(FileServerTest, DeletePathTraversalRejected) {
    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_DELETE;
    req->url    = Url("/api/v1/files/..%2F..%2Fetc%2Fpasswd");
    auto resp   = requests::request(req);
    ASSERT_NE(resp, nullptr);
    // 400 (path safety) 或 404 (不存在) 均可，不应为 200
    EXPECT_NE(resp->status_code, 200);
}

TEST_F(FileServerTest, UploadMultipartPathTraversalRejected) {
    // 手工构造 multipart body，绕过 libhv 的 basename 处理
    // 模拟恶意客户端发送 filename="../../evil.txt"
    std::string boundary = "----TestBoundary12345";
    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"../../evil.txt\"\r\n";
    body += "\r\n";
    body += "evil content\r\n";
    body += "--" + boundary + "--\r\n";

    auto req    = std::make_shared<HttpRequest>();
    req->method = HTTP_POST;
    req->url    = Url("/api/v1/files/upload");
    req->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    req->body   = body;
    auto resp = requests::request(req);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 400);
    // 确保文件没有被写到 root_dir 之外
    EXPECT_FALSE(fs::exists(tmp_dir_ + "/../../evil.txt"));
}

// ============================================================
// 7. 大文件流式上传
// ============================================================

TEST_F(FileServerTest, LargeUploadSuccess) {
    // 生成一段测试数据
    std::string body(4096, 'A');  // 4KB
    auto url  = Url("/api/v1/files/upload/large_test.zip");
    auto resp = requests::post(url.c_str(), body);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status_code, 200);

    // 验证文件存在且大小正确
    std::string filepath = tmp_dir_ + "/large_test.zip";
    EXPECT_TRUE(fs::exists(filepath));
    if (fs::exists(filepath)) {
        EXPECT_EQ(fs::file_size(filepath), body.size());
    }
}

TEST_F(FileServerTest, LargeUploadPathTraversalRejected) {
    std::string body = "evil";
    auto url         = Url("/api/v1/files/upload/..%2F..%2Fevil.txt");
    auto resp        = requests::post(url.c_str(), body);
    // 无论状态码如何，验证文件不会创建在 root_dir 之外
    EXPECT_FALSE(fs::exists(tmp_dir_ + "/../evil.txt"));
    EXPECT_FALSE(fs::exists(tmp_dir_ + "/../../evil.txt"));
    // 清理可能在 root_dir 内创建的文件
    std::error_code ec;
    fs::remove(tmp_dir_ + "/..%2F..%2Fevil.txt", ec);
}

// ============================================================
// 8. 静态文件路径穿越
// ============================================================

TEST_F(FileServerTest, StaticPathTraversal) {
    auto url  = Url("/static/../../../etc/passwd");
    auto resp = requests::get(url.c_str());
    ASSERT_NE(resp, nullptr);
    // 不应返回 200
    EXPECT_NE(resp->status_code, 200);
}

}  // namespace
}  // namespace nova
