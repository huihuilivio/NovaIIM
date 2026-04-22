#include "file_server.h"
#include "../core/logger.h"

#include <hv/hfile.h>
#include <hv/json.hpp>
#include <hv/htime.h>

#include <chrono>
#include <filesystem>
#include <thread>

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
// sendLargeFile（参考 libhv httpd demo）
// 由 service_.largeFileHandler 自动触发：
//   当 /static 路径的文件 > max_file_cache_size(4MB) 时
// ============================================================

static int sendLargeFile(const HttpContextPtr& ctx) {
    std::thread([ctx]() {
        ctx->writer->Begin();
        // 使用 GetStaticFilepath 解析 /static/xxx → root_dir/xxx
        std::string filepath = ctx->service->GetStaticFilepath(ctx->request->Path().c_str());
        if (filepath.empty()) {
            filepath = ctx->service->document_root + ctx->request->Path();
        }
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
        int len   = 40960;  // 40K
        SAFE_ALLOC(buf, len);
        size_t total_readbytes = 0;
        int sleep_ms_per_send  = 0;
        if (ctx->service->limit_rate <= 0) {
            // unlimited
        } else {
            sleep_ms_per_send = len * 1000 / 1024 / ctx->service->limit_rate;
        }
        if (sleep_ms_per_send == 0)
            sleep_ms_per_send = 1;
        int sleep_ms    = sleep_ms_per_send;
        auto start_time = std::chrono::steady_clock::now();
        auto end_time   = start_time;
        while (total_readbytes < filesize) {
            if (!ctx->writer->isConnected()) {
                break;
            }
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
            if (nwrite < 0) {
                break;
            }
            total_readbytes += readbytes;
            end_time += std::chrono::milliseconds(sleep_ms);
            std::this_thread::sleep_until(end_time);
        }
        ctx->writer->End();
        SAFE_FREE(buf);
    }).detach();
    return HTTP_STATUS_UNFINISHED;
}

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
    if (server_) {
        server_->stop();
        server_.reset();
        NOVA_NLOG_INFO(kLogTag, "stopped");
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
    service_.largeFileHandler = sendLargeFile;

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
            // 用安全文件名保存
            std::string filepath = save_path + filename;
            HFile file;
            if (file.open(filepath.c_str(), "wb") != 0) {
                return response_status(ctx, HTTP_STATUS_INTERNAL_SERVER_ERROR, "cannot create file");
            }
            file.write(it->second.content.data(), it->second.content.size());
            NOVA_NLOG_INFO(kLogTag, "multipart upload '{}' ok", filename);
        } else {
            std::string filename = ctx->param("filename", "unnamed.txt");
            if (!IsPathSafe(filename)) {
                NOVA_NLOG_WARN(kLogTag, "upload rejected: invalid filename '{}'", filename);
                return response_status(ctx, HTTP_STATUS_BAD_REQUEST, "invalid filename");
            }
            std::string filepath = save_path + filename;
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
        [root_dir, max_upload_size](const HttpContextPtr& ctx, http_parser_state state,
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
                // Content-Length 检查
                int64_t content_len = ctx->request->ContentLength();
                if (max_upload_size > 0 && content_len > max_upload_size) {
                    ctx->close();
                    return HTTP_STATUS_BAD_REQUEST;
                }
                std::string filepath = root_dir + "/" + filename;
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
        if (ec || ec2 || canonical.string().find(root_canonical.string()) != 0) {
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
