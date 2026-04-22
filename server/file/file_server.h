#pragma once

#include <hv/HttpService.h>
#include <hv/HttpServer.h>

#include <memory>
#include <string>

namespace nova {

/// HTTP 文件服务器
/// 功能：静态文件预览/下载（含大文件）、小文件上传、大文件流式上传
class FileServer {
public:
    struct Options {
        int port              = 9092;   // 监听端口
        std::string root_dir  = "files"; // 文件存储根目录
        int64_t max_upload_size = 500LL * 1024 * 1024; // 上传文件大小上限（默认 500MB）
        int limit_rate        = -1;     // 下载限速 KB/s，-1 不限速
    };

    FileServer() = default;
    ~FileServer() = default;

    FileServer(const FileServer&) = delete;
    FileServer& operator=(const FileServer&) = delete;

    int  Start(const Options& opts);
    void Stop();

private:
    void RegisterRoutes();

    // ---- 路径安全 ----
    bool IsPathSafe(const std::string& path) const;

    Options opts_;
    hv::HttpService service_;
    std::unique_ptr<hv::HttpServer> server_;
};

}  // namespace nova
