#include "file_server.h"
#include "../core/logger.h"

#include <hv/hfile.h>
#include <hv/json.hpp>
#include <hv/htime.h>

#include <chrono>
#include <cctype>
#include <filesystem>
#include <thread>
#include <unordered_set>

namespace nova {

namespace fs = std::filesystem;

static constexpr const char* kLogTag = "FileServer";

// ============================================================
// response_status（参考 libhv httpd demo）
// ============================================================

static int response_status(HttpResponse* resp, int code = 200, const char* message = NULL) {
    if (message == NULL)
        message = http_status_str((enum http_status)code);
    resp->status_code = (http_status)code;
    resp->Set("code", code);
    resp->Set("message", message);
    return code;
}

static int response_status(const HttpContextPtr& ctx, int code = 200, const char* message = NULL) {
    response_status(ctx->response.get(), code, message);
    ctx->send();
    return code;
}

// ============================================================
// 大文件下载（成员方法，替代原先 detach 版本）
// 调用条件：/static 路径的文件 > max_file_cache_size(4MB)
// 设计：
//   - 不再使用 std::thread::detach()，所有下载线程追踪于 downloads_ 列表；
//   - Stop() join 所有线程，避免关停时中断传输、数据丢失；
//   - symlink 检查：拒绝任何经符号链接诱导到根目录外的文件。
// ============================================================

// ============================================================
// 路径安全
// ============================================================

bool FileServer::IsPathSafe(const std::string& path) const {
    if (path.empty())
        return false;
    if (path.find("..") != std::string::npos)
        return false;
    if (path[0] == '/' || path[0] == '\\')
        return false;
#ifdef _WIN32
    if (path.size() >= 2 && path[1] == ':')
        return false;
#endif
    if (path.find('\0') != std::string::npos)
        return false;
    return true;
}

// 校验目标文件路径、其父链上不出现 symlink，并确保解析后的真实路径仍在 root_dir 内。
bool FileServer::IsDestinationSafe(const std::string& filepath) const {
    std::error_code ec;
    fs::path p(filepath);
    fs::path root = fs::absolute(opts_.root_dir, ec);
    if (ec) return false;
    root = fs::weakly_canonical(root, ec);
    if (ec) return false;

    // 拒绝已存在的 symlink（上传/下载目标都适用）
    if (fs::exists(p, ec) && fs::is_symlink(p, ec)) {
        return false;
    }
    // 父链判断：向上回溯到 root，任何一段是 symlink 则拒绝
    auto parent = p.parent_path();
    while (!parent.empty()) {
        if (fs::exists(parent, ec) && fs::is_symlink(parent, ec)) {
            return false;
        }
        auto abs_parent = fs::weakly_canonical(parent, ec);
        if (!ec && abs_parent == root) break;
        auto next = parent.parent_path();
        if (next == parent) break;  // reached filesystem root
        parent = next;
    }
    // 解析后的实际路径必须仍在 root 内
    auto canon = fs::weakly_canonical(p, ec);
    if (ec) return false;
    auto [root_end, _] = std::mismatch(root.begin(), root.end(), canon.begin(), canon.end());
    if (root_end != root.end()) return false;
    return true;
}

// 纵深防御：仅允许白名单扩展名（小写比较），避免 .exe/.bat/.sh/.dll/.jsp/.php 等可执行/脚本被托管。
// 若 root_dir 被配置为其他 web 服务的静态目录，这里阻止上传导致 RCE 的后缀。
bool FileServer::IsExtensionAllowed(const std::string& filename) const {
    auto pos = filename.find_last_of('.');
    if (pos == std::string::npos) return false;  // 无扩展名：拒绝
    std::string ext = filename.substr(pos + 1);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::unordered_set<std::string> kAllowed = {
        // 图片
        "jpg", "jpeg", "png", "gif", "webp", "bmp", "svg", "ico",
        // 文档
        "pdf", "txt", "md", "csv", "log",
        "doc", "docx", "xls", "xlsx", "ppt", "pptx",
        // 音视频
        "mp3", "wav", "ogg", "flac", "m4a",
        "mp4", "webm", "mov", "avi", "mkv",
        // 压缩
        "zip", "rar", "7z", "tar", "gz", "bz2",
        // 数据
        "json", "xml", "yaml", "yml",
    };
    return kAllowed.count(ext) != 0;
}

// ============================================================
// 下载线程追踪（counter + cv）
// 设计：
//   - 下载线程 detach，但每个件 inflight 计数；
//   - 完成时递减并 notify；
//   - Stop() 等待 inflight 归零（有超时保护）避免资源使用后的释放。
// ============================================================

bool FileServer::LaunchDownloadThread(std::function<void()> job) {
    if (stopping_.load()) {
        NOVA_NLOG_WARN(kLogTag, "download rejected: server stopping");
        return false;
    }
    // 并发限制：避免线程爆炸
    size_t cur = downloads_inflight_.load(std::memory_order_relaxed);
    do {
        if (cur >= kMaxConcurrentDownloads) {
            NOVA_NLOG_WARN(kLogTag, "download rejected: inflight {} >= max {}",
                           cur, kMaxConcurrentDownloads);
            return false;
        }
    } while (!downloads_inflight_.compare_exchange_weak(cur, cur + 1,
                                                        std::memory_order_acq_rel));

    try {
        std::thread([this, job = std::move(job)]() {
            try {
                job();
            } catch (...) {
                NOVA_NLOG_ERROR(kLogTag, "download thread threw");
            }
            if (downloads_inflight_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                // 最后一个下载完成，售醒 Stop()
                std::lock_guard<std::mutex> lk(downloads_mu_);
                downloads_cv_.notify_all();
            }
        }).detach();
    } catch (const std::system_error& e) {
        // std::thread 构造失败（如系统线程资源耗尽）：回滚 inflight 计数，避免泄漏导致 Stop() 永久挂起
        if (downloads_inflight_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lk(downloads_mu_);
            downloads_cv_.notify_all();
        }
        NOVA_NLOG_ERROR(kLogTag, "failed to spawn download thread: {}", e.what());
        return false;
    }
    return true;
}

int FileServer::SubmitLargeFileDownload(const HttpContextPtr& ctx) {
    // 在提交前解析并校验路径（symlink 防护）
    std::string filepath = ctx->service->GetStaticFilepath(ctx->request->Path().c_str());
    if (filepath.empty()) {
        filepath = ctx->service->document_root + ctx->request->Path();
    }
    if (!IsDestinationSafe(filepath)) {
        NOVA_NLOG_WARN(kLogTag, "large file rejected (symlink/outside root): {}", filepath);
        ctx->writer->Begin();
        ctx->writer->WriteStatus(HTTP_STATUS_FORBIDDEN);
        ctx->writer->WriteHeader("Content-Type", "text/html");
        ctx->writer->WriteBody("<center><h1>403 Forbidden</h1></center>");
        ctx->writer->End();
        return HTTP_STATUS_UNFINISHED;
    }

    bool launched = LaunchDownloadThread([ctx, filepath]() {
        ctx->writer->Begin();
        HFile file;
        if (file.open(filepath.c_str(), "rb") != 0) {
            NOVA_NLOG_WARN(kLogTag, "large file not found: {}", filepath);
            ctx->writer->WriteStatus(HTTP_STATUS_NOT_FOUND);
            ctx->writer->WriteHeader("Content-Type", "text/html");
            ctx->writer->WriteBody("<center><h1>404 Not Found</h1></center>");
            ctx->writer->End();
            return;
        }
        NOVA_NLOG_INFO(kLogTag, "large file download: {} ({} bytes)", filepath, file.size());
        http_content_type content_type = CONTENT_TYPE_NONE;
        const char* suffix             = hv_suffixname(filepath.c_str());
        if (suffix) {
            content_type = http_content_type_enum_by_suffix(suffix);
        }
        if (content_type == CONTENT_TYPE_NONE || content_type == CONTENT_TYPE_UNDEFINED) {
            content_type = APPLICATION_OCTET_STREAM;
        }
        size_t filesize = file.size();
        ctx->writer->WriteStatus(HTTP_STATUS_OK);
        ctx->writer->WriteHeader("Content-Type", http_content_type_str(content_type));
        ctx->writer->WriteHeader("Content-Length", filesize);
        ctx->writer->EndHeaders();

        char* buf = NULL;
        int len   = 40960;
        SAFE_ALLOC(buf, len);
        size_t total_readbytes = 0;
        int sleep_ms_per_send  = 0;
        if (ctx->service->limit_rate > 0) {
            sleep_ms_per_send = len * 1000 / 1024 / ctx->service->limit_rate;
        }
        if (sleep_ms_per_send == 0) sleep_ms_per_send = 1;
        int sleep_ms    = sleep_ms_per_send;
        auto start_time = std::chrono::steady_clock::now();
        auto end_time   = start_time;
        while (total_readbytes < filesize) {
            if (!ctx->writer->isConnected()) break;
            if (!ctx->writer->isWriteComplete()) {
                hv_delay(1);
                continue;
            }
            size_t readbytes = file.read(buf, len);
            if (readbytes == 0) {
                ctx->writer->close();
                break;
            }
            int nwrite = ctx->writer->WriteBody(buf, readbytes);
            if (nwrite < 0) break;
            total_readbytes += readbytes;
            end_time += std::chrono::milliseconds(sleep_ms);
            std::this_thread::sleep_until(end_time);
        }
        ctx->writer->End();
        SAFE_FREE(buf);
    });
    if (!launched) {
        // 被限流/停机拒绝：必须向客户端返回响应，否则连接会挂起
        ctx->writer->Begin();
        ctx->writer->WriteStatus(HTTP_STATUS_SERVICE_UNAVAILABLE);
        ctx->writer->WriteHeader("Content-Type", "text/html");
        ctx->writer->WriteBody("<center><h1>503 Service Unavailable</h1></center>");
        ctx->writer->End();
    }
    return HTTP_STATUS_UNFINISHED;
}

// ============================================================
// Start / Stop
// ============================================================

int FileServer::Start(const Options& opts) {
    opts_ = opts;

    // 确保根目录存在
    std::error_code ec;
    fs::create_directories(opts_.root_dir, ec);
    if (ec) {
        NOVA_NLOG_ERROR(kLogTag, "cannot create root dir '{}': {}", opts_.root_dir, ec.message());
        return -1;
    }

    RegisterRoutes();

    server_ = std::make_unique<hv::HttpServer>(&service_);
    server_->setThreadNum(2);
    server_->setPort(opts_.port);

    int ret = server_->start();
    if (ret == 0) {
        NOVA_NLOG_INFO(kLogTag, "File server started on port {}, root={}", opts_.port, opts_.root_dir);
    } else {
        NOVA_NLOG_ERROR(kLogTag, "File server failed to start on port {}", opts_.port);
    }
    return ret;
}

void FileServer::Stop() {
    stopping_.store(true);
    if (server_) {
        server_->stop();
        server_.reset();
    }
    // 等待所有 inflight 下载完成，避免数据丢失 / writer 释放后访问
    // 超时保护：防奇极情况下 Stop 被挂住不能退出
    {
        std::unique_lock<std::mutex> lk(downloads_mu_);
        downloads_cv_.wait_for(lk, std::chrono::seconds(30),
                               [this] { return downloads_inflight_.load() == 0; });
    }
    auto remaining = downloads_inflight_.load();
    if (remaining == 0) {
        NOVA_NLOG_INFO(kLogTag, "stopped (all downloads completed)");
    } else {
        NOVA_NLOG_WARN(kLogTag, "stopped with {} downloads still running (timed out)", remaining);
    }
}

// ============================================================
// 路由注册
// ============================================================

void FileServer::RegisterRoutes() {
    // ---- 静态文件预览/下载（将 /static/** 映射到 root_dir） ----
    // 小文件自动走 FileCache，大文件 (>4MB) 自动触发 largeFileHandler
    service_.Static("/static", opts_.root_dir.c_str());
    service_.document_root    = opts_.root_dir;
    service_.limit_rate       = opts_.limit_rate;
    service_.largeFileHandler = [this](const HttpContextPtr& ctx) {
        return SubmitLargeFileDownload(ctx);
    };

    // ---- 健康检查 ----
    service_.GET("/healthz", [](HttpRequest*, HttpResponse* resp) {
        resp->content_type = APPLICATION_JSON;
        resp->body         = R"({"status":"ok"})";
        return 200;
    });

    // ---- 小文件上传（multipart/form-data 或 raw body） ----
    // curl -v http://ip:port/api/v1/files/upload -F 'file=@test.txt'
    // curl -v http://ip:port/api/v1/files/upload?filename=test.txt -d '@test.txt'
    service_.POST("/api/v1/files/upload", [this](const HttpContextPtr& ctx) {
        // Content-Length 检查
        int64_t content_len = ctx->request->ContentLength();
        if (opts_.max_upload_size > 0 && content_len > opts_.max_upload_size) {
            NOVA_NLOG_WARN(kLogTag, "upload rejected: size {} exceeds limit {}", content_len, opts_.max_upload_size);
            return response_status(ctx, HTTP_STATUS_BAD_REQUEST, "file too large");
        }

        int status_code       = 200;
        std::string save_path = opts_.root_dir + "/";
        if (ctx->is(MULTIPART_FORM_DATA)) {
            // multipart: 先解析表单，校验文件名安全性
            auto& form = ctx->request->form;
            if (form.empty()) {
                ctx->request->ParseBody();
            }
            auto it = form.find("file");
            if (it == form.end() || it->second.content.empty()) {
                return response_status(ctx, HTTP_STATUS_BAD_REQUEST, "missing file field");
            }
            // 先校验原始文件名，拒绝包含 ".." 等危险路径
            std::string raw_filename = it->second.filename;
            if (!IsPathSafe(raw_filename)) {
                NOVA_NLOG_WARN(kLogTag, "multipart upload rejected: invalid filename '{}'", raw_filename);
                return response_status(ctx, HTTP_STATUS_BAD_REQUEST, "invalid filename");
            }
            // 取 basename 作为最终文件名
            std::string filename = raw_filename;
            auto pos = filename.find_last_of("/\\");
            if (pos != std::string::npos) {
                filename = filename.substr(pos + 1);
            }
            if (filename.empty()) {
                return response_status(ctx, HTTP_STATUS_BAD_REQUEST, "empty filename");
            }
            if (!IsExtensionAllowed(filename)) {
                NOVA_NLOG_WARN(kLogTag, "multipart upload rejected: extension not allowed '{}'", filename);
                return response_status(ctx, HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, "file type not allowed");
            }
            // 用安全文件名保存
            std::string filepath = save_path + filename;
            if (!IsDestinationSafe(filepath)) {
                NOVA_NLOG_WARN(kLogTag, "multipart upload rejected (symlink/outside root): {}", filepath);
                return response_status(ctx, HTTP_STATUS_FORBIDDEN, "unsafe destination");
            }
            HFile file;
            if (file.open(filepath.c_str(), "wb") != 0) {
                return response_status(ctx, HTTP_STATUS_INTERNAL_SERVER_ERROR, "cannot create file");
            }
            if (file.write(it->second.content.data(), it->second.content.size()) != it->second.content.size()) {
                return response_status(ctx, HTTP_STATUS_INTERNAL_SERVER_ERROR, "write failed");
            }
            NOVA_NLOG_INFO(kLogTag, "multipart upload '{}' ok", filename);
        } else {
            std::string filename = ctx->param("filename", "unnamed.txt");
            if (!IsPathSafe(filename)) {
                NOVA_NLOG_WARN(kLogTag, "upload rejected: invalid filename '{}'", filename);
                return response_status(ctx, HTTP_STATUS_BAD_REQUEST, "invalid filename");
            }
            if (!IsExtensionAllowed(filename)) {
                NOVA_NLOG_WARN(kLogTag, "raw upload rejected: extension not allowed '{}'", filename);
                return response_status(ctx, HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, "file type not allowed");
            }
            std::string filepath = save_path + filename;
            if (!IsDestinationSafe(filepath)) {
                NOVA_NLOG_WARN(kLogTag, "upload rejected (symlink/outside root): {}", filepath);
                return response_status(ctx, HTTP_STATUS_FORBIDDEN, "unsafe destination");
            }
            status_code          = ctx->request->SaveFile(filepath.c_str());
            NOVA_NLOG_INFO(kLogTag, "raw upload '{}' status={}", filename, status_code);
        }
        return response_status(ctx, status_code);
    });

    // ---- 大文件流式上传（文件名在 URL 路径中） ----
    // curl -v http://ip:port/api/v1/files/upload/bigfile.zip -d '@bigfile.zip'
    std::string root_dir      = opts_.root_dir;
    int64_t max_upload_size   = opts_.max_upload_size;
    service_.POST("/api/v1/files/upload/{filename}",
        [this, root_dir, max_upload_size](const HttpContextPtr& ctx, http_parser_state state,
                                     const char* data, size_t size) -> int {
            int status_code = HTTP_STATUS_UNFINISHED;
            HFile* file     = (HFile*)ctx->userdata;
            switch (state) {
            case HP_HEADERS_COMPLETE: {
                if (ctx->is(MULTIPART_FORM_DATA)) {
                    ctx->close();
                    return HTTP_STATUS_BAD_REQUEST;
                }
                std::string filename = ctx->param("filename", "unnamed.txt");
                // 路径安全检查：禁止 ".." 和绝对路径
                if (filename.find("..") != std::string::npos ||
                    filename.find('/') != std::string::npos ||
                    filename.find('\\') != std::string::npos) {
                    NOVA_NLOG_WARN(kLogTag, "large upload rejected: unsafe filename '{}'", filename);
                    ctx->close();
                    return HTTP_STATUS_BAD_REQUEST;
                }
                if (!IsExtensionAllowed(filename)) {
                    NOVA_NLOG_WARN(kLogTag, "large upload rejected: extension not allowed '{}'", filename);
                    ctx->close();
                    return HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE;
                }
                // Content-Length 检查
                int64_t content_len = ctx->request->ContentLength();
                if (max_upload_size > 0 && content_len > max_upload_size) {
                    NOVA_NLOG_WARN(kLogTag, "large upload rejected: size {} exceeds limit {}",
                                   content_len, max_upload_size);
                    ctx->close();
                    return HTTP_STATUS_BAD_REQUEST;
                }
                std::string filepath = root_dir + "/" + filename;
                if (!IsDestinationSafe(filepath)) {
                    NOVA_NLOG_WARN(kLogTag, "large upload rejected (symlink/outside root): {}", filepath);
                    ctx->close();
                    return HTTP_STATUS_FORBIDDEN;
                }
                file                 = new HFile;
                if (file->open(filepath.c_str(), "wb") != 0) {
                    delete file;
                    ctx->close();
                    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                }
                ctx->userdata = file;
            } break;
            case HP_BODY: {
                if (file && data && size) {
                    if (file->write(data, size) != size) {
                        file->remove();
                        delete file;
                        ctx->userdata = NULL;
                        ctx->close();
                        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                    }
                }
            } break;
            case HP_MESSAGE_COMPLETE: {
                status_code = HTTP_STATUS_OK;
                ctx->setContentType(APPLICATION_JSON);
                response_status(ctx->response.get(), status_code);
                ctx->send();
                NOVA_NLOG_INFO(kLogTag, "large upload complete: {}/{}",
                               root_dir, ctx->param("filename", "?"));
                if (file) {
                    delete file;
                    ctx->userdata = NULL;
                }
            } break;
            case HP_ERROR: {
                if (file) {
                    file->remove();
                    delete file;
                    ctx->userdata = NULL;
                }
            } break;
            default:
                break;
            }
            return status_code;
        });

    // ---- 文件删除 ----
    service_.Delete("/api/v1/files/{filename}", [this](const HttpContextPtr& ctx) {
        std::string filename = ctx->param("filename");
        if (!IsPathSafe(filename)) {
            return response_status(ctx, HTTP_STATUS_BAD_REQUEST, "invalid filename");
        }

        std::string full_path = (fs::path(opts_.root_dir) / filename).string();

        if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
            return response_status(ctx, HTTP_STATUS_NOT_FOUND, "file not found");
        }

        // 规范化后确保在 root_dir 内
        std::error_code ec, ec2;
        auto canonical      = fs::canonical(full_path, ec);
        auto root_canonical = fs::canonical(opts_.root_dir, ec2);
        auto root_prefix    = root_canonical.string() + std::string(1, fs::path::preferred_separator);
        if (ec || ec2 || !canonical.string().starts_with(root_prefix)) {
            return response_status(ctx, HTTP_STATUS_BAD_REQUEST, "invalid path");
        }

        fs::remove(full_path, ec);
        if (ec) {
            NOVA_NLOG_ERROR(kLogTag, "delete failed: {} - {}", full_path, ec.message());
            return response_status(ctx, HTTP_STATUS_INTERNAL_SERVER_ERROR, "failed to delete");
        }

        NOVA_NLOG_INFO(kLogTag, "file deleted: {}", filename);
        return response_status(ctx, HTTP_STATUS_OK);
    });
}

}  // namespace nova
