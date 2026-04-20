#pragma once
// FileService — 文件上传/下载相关服务

#include <viewmodel/types.h>

#include <functional>
#include <string>

namespace nova::client {

class ClientContext;

class FileService {
public:
    using UploadCallback         = std::function<void(const UploadResult&)>;
    using UploadCompleteCallback = std::function<void(const UploadCompleteResult&)>;
    using DownloadCallback       = std::function<void(const DownloadResult&)>;

    explicit FileService(ClientContext& ctx);

    void RequestUpload(const std::string& file_name, int64_t file_size,
                       const std::string& mime_type, const std::string& file_hash,
                       const std::string& file_type, UploadCallback cb);
    void UploadComplete(int64_t file_id, UploadCompleteCallback cb);
    void RequestDownload(int64_t file_id, bool thumb, DownloadCallback cb);

private:
    ClientContext& ctx_;
};

}  // namespace nova::client
