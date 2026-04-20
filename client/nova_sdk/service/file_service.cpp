#include "file_service.h"
#include "service_helpers.h"

namespace nova::client {

using namespace detail;

FileService::FileService(ClientContext& ctx) : ctx_(ctx) {}

void FileService::RequestUpload(const std::string& file_name, int64_t file_size,
                            const std::string& mime_type, const std::string& file_hash,
                            const std::string& file_type, UploadCallback cb) {
    nova::proto::UploadReq req;
    req.file_name = file_name;
    req.file_size = file_size;
    req.mime_type = mime_type;
    req.file_hash = file_hash;
    req.file_type = file_type;

    auto pkt = MakePacket(nova::proto::Cmd::kUploadReq, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::UploadAckMsg>(ctx_, pkt,
        [cb](const std::optional<nova::proto::UploadAckMsg>& ack) {
            UploadResult result;
            if (ack && ack->code == 0) {
                result.success        = true;
                result.file_id        = ack->file_id;
                result.upload_url     = ack->upload_url;
                result.already_exists = (ack->already_exists != 0);
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void FileService::UploadComplete(int64_t file_id, UploadCompleteCallback cb) {
    nova::proto::UploadCompleteReq req;
    req.file_id = file_id;

    auto pkt = MakePacket(nova::proto::Cmd::kUploadComplete, ctx_.NextSeq(), req);

    SendRequest<nova::proto::UploadCompleteAckMsg>(ctx_, pkt,
        [cb](const std::optional<nova::proto::UploadCompleteAckMsg>& ack) {
            UploadCompleteResult result;
            if (ack && ack->code == 0) {
                result.success   = true;
                result.file_path = ack->file_path;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void FileService::RequestDownload(int64_t file_id, bool thumb, DownloadCallback cb) {
    nova::proto::DownloadReq req;
    req.file_id = file_id;
    req.thumb   = thumb ? 1 : 0;

    auto pkt = MakePacket(nova::proto::Cmd::kDownloadReq, ctx_.NextSeq(), req);

    SendRequest<nova::proto::DownloadAckMsg>(ctx_, pkt,
        [cb](const std::optional<nova::proto::DownloadAckMsg>& ack) {
            DownloadResult result;
            if (ack && ack->code == 0) {
                result.success      = true;
                result.download_url = ack->download_url;
                result.file_name    = ack->file_name;
                result.file_size    = ack->file_size;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

}  // namespace nova::client
