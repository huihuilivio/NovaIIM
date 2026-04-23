#pragma once

#include <hv/HttpService.h>
#include <hv/HttpServer.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
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
    ~FileServer() { Stop(); }

    FileServer(const FileServer&) = delete;
    FileServer& operator=(const FileServer&) = delete;

    int  Start(const Options& opts);
    void Stop();

private:
    void RegisterRoutes();

    // ---- 路径安全 ----
    bool IsPathSafe(const std::string& path) const;
    // 校验目标文件/其父目录不是符号链接，防止符号链接穿越写入/读取
    bool IsDestinationSafe(const std::string& filepath) const;

    // 启动 detach 下载线程；inflight 计数使 Stop() 可等待全部完成避免数据丢失
    // 返回 false 表示被限流拒绝（调用方需向客户端返回错误）
    bool LaunchDownloadThread(std::function<void()> job);
    int  SubmitLargeFileDownload(const HttpContextPtr& ctx);

    Options opts_;
    hv::HttpService service_;
    std::unique_ptr<hv::HttpServer> server_;

    // 下载并发控制（counter + cv 代替 thread 列表，避免已完成线程占用限额）
    static constexpr size_t kMaxConcurrentDownloads = 32;
    std::mutex                  downloads_mu_;
    std::condition_variable     downloads_cv_;
    std::atomic<size_t>         downloads_inflight_{0};
    std::atomic<bool>           stopping_{false};
};

}  // namespace nova
