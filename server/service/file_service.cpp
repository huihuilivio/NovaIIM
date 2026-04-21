#include "file_service.h"
#include "../core/logger.h"
#include "../dao/file_dao.h"
#include "../dao/conversation_dao.h"
#include <nova/errors.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <unordered_set>

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "FileSvc";
static constexpr int64_t kMaxFileSize = 100 * 1024 * 1024;  // 100 MB

// ---- Upload ----

void FileService::HandleUpload(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::UploadReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    // 校验
    if (req->file_name.empty()) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::file::kFileNameRequired.code, ec::file::kFileNameRequired.msg});
        return;
    }
    if (req->file_name.size() > 512) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::kInvalidBody.code, "file_name too long"});
        return;
    }
    if (req->file_hash.size() > 256) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::kInvalidBody.code, "file_hash too long"});
        return;
    }
    if (req->file_size <= 0) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::file::kFileSizeInvalid.code, ec::file::kFileSizeInvalid.msg});
        return;
    }
    if (req->file_size > kMaxFileSize) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::file::kFileSizeTooLarge.code, ec::file::kFileSizeTooLarge.msg});
        return;
    }
    if (req->mime_type.empty()) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::file::kMimeTypeRequired.code, ec::file::kMimeTypeRequired.msg});
        return;
    }
    if (req->mime_type.size() > 128) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::kInvalidBody.code, "mime_type too long"});
        return;
    }

    // 校验 file_type 白名单（防止 DAO 层 raw SQL 注入）
    std::string file_type = req->file_type.empty() ? "file" : req->file_type;
    if (file_type != "avatar" && file_type != "image" && file_type != "voice" &&
        file_type != "video" && file_type != "file") {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::file::kInvalidFileType.code, ec::file::kInvalidFileType.msg});
        return;
    }

    // 秒传检测：hash 匹配
    if (!req->file_hash.empty()) {
        auto existing = ctx_.dao().File().FindLatestByUserAndType(user_id, file_type);
        if (existing && existing->hash == req->file_hash) {
            proto::UploadAckMsg ack;
            ack.code           = ec::kOk.code;
            ack.msg            = ec::kOk.msg;
            ack.file_id        = existing->id;
            ack.upload_url     = existing->file_path;
            ack.already_exists = 1;
            SendPacket(conn, Cmd::kUploadAck, seq, 0, ack);
            return;
        }
    }

    // 创建文件元数据记录
    UserFile file;
    file.user_id   = user_id;
    file.file_type = file_type;
    file.file_name = req->file_name;
    file.file_size = req->file_size;
    file.mime_type = req->mime_type;
    file.hash      = req->file_hash;

    // 防止路径穿越：只取文件名部分，去除目录分隔符
    std::string safe_name = req->file_name;
    {
        auto pos1 = safe_name.rfind('/');
        auto pos2 = safe_name.rfind('\\');
        size_t pos = std::string::npos;
        if (pos1 != std::string::npos && pos2 != std::string::npos)
            pos = std::max(pos1, pos2);
        else if (pos1 != std::string::npos)
            pos = pos1;
        else
            pos = pos2;
        if (pos != std::string::npos)
            safe_name = safe_name.substr(pos + 1);
    }
    if (safe_name.empty() || safe_name == "." || safe_name == "..") {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::file::kFileNameRequired.code, ec::file::kFileNameRequired.msg});
        return;
    }

    // 生成存储路径
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf {};
#ifdef _MSC_VER
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char date_buf[16];
    std::strftime(date_buf, sizeof(date_buf), "%Y%m%d", &tm_buf);

    // 先插入获取 file_id，再生成唯一路径（避免同名文件覆盖）
    file.file_path = "pending";
    if (!ctx_.dao().File().Insert(file)) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::file::kUploadFailed.code, ec::file::kUploadFailed.msg});
        return;
    }

    std::string rel_path = "uploads/" + std::string(date_buf) + "/" +
                           std::to_string(file.id) + "_" + std::to_string(user_id) + "_" + safe_name;
    file.file_path = rel_path;
    // 更新路径
    if (!ctx_.dao().File().UpdatePath(file.id, rel_path)) {
        SendPacket(conn, Cmd::kUploadAck, seq, 0,
                   proto::UploadAckMsg{ec::file::kUploadFailed.code, ec::file::kUploadFailed.msg});
        return;
    }

    proto::UploadAckMsg ack;
    ack.code       = ec::kOk.code;
    ack.msg        = ec::kOk.msg;
    ack.file_id    = file.id;
    ack.upload_url = rel_path;
    SendPacket(conn, Cmd::kUploadAck, seq, 0, ack);

    NOVA_NLOG_INFO(kLogTag, "upload started: user={}, file_id={}, name={}", user_id, file.id, req->file_name);
}

// ---- UploadComplete ----

void FileService::HandleUploadComplete(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kUploadCompleteAck, seq, 0,
                   proto::UploadCompleteAckMsg{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::UploadCompleteReq>(pkt.body);
    if (!req || req->file_id <= 0) {
        SendPacket(conn, Cmd::kUploadCompleteAck, seq, 0,
                   proto::UploadCompleteAckMsg{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto file = ctx_.dao().File().FindById(req->file_id);
    if (!file || file->user_id != user_id) {
        SendPacket(conn, Cmd::kUploadCompleteAck, seq, 0,
                   proto::UploadCompleteAckMsg{ec::file::kFileNotFound.code, ec::file::kFileNotFound.msg});
        return;
    }

    proto::UploadCompleteAckMsg ack;
    ack.code      = ec::kOk.code;
    ack.msg       = ec::kOk.msg;
    ack.file_path = file->file_path;
    SendPacket(conn, Cmd::kUploadCompleteAck, seq, 0, ack);

    NOVA_NLOG_INFO(kLogTag, "upload complete: user={}, file_id={}", user_id, req->file_id);
}

// ---- Download ----

void FileService::HandleDownload(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kDownloadAck, seq, 0,
                   proto::DownloadAckMsg{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::DownloadReq>(pkt.body);
    if (!req || req->file_id <= 0) {
        SendPacket(conn, Cmd::kDownloadAck, seq, 0,
                   proto::DownloadAckMsg{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto file = ctx_.dao().File().FindById(req->file_id);
    if (!file) {
        SendPacket(conn, Cmd::kDownloadAck, seq, 0,
                   proto::DownloadAckMsg{ec::file::kFileNotFound.code, ec::file::kFileNotFound.msg});
        return;
    }
    if (file->user_id != user_id) {
        // 允许下载：如果请求者与文件所有者共处某个会话（即通过 IM 收到了文件）
        auto my_memberships    = ctx_.dao().Conversation().GetMembersByUser(user_id);
        auto owner_memberships = ctx_.dao().Conversation().GetMembersByUser(file->user_id);
        std::unordered_set<int64_t> owner_convs;
        for (const auto& m : owner_memberships) owner_convs.insert(m.conversation_id);
        bool shares_conv = false;
        for (const auto& m : my_memberships) {
            if (owner_convs.count(m.conversation_id)) { shares_conv = true; break; }
        }
        if (!shares_conv) {
            NOVA_NLOG_WARN(kLogTag, "unauthorized download: user={}, file_id={}, owner={}",
                           user_id, req->file_id, file->user_id);
            SendPacket(conn, Cmd::kDownloadAck, seq, 0,
                       proto::DownloadAckMsg{ec::file::kFileNotFound.code, ec::file::kFileNotFound.msg});
            return;
        }
    }

    proto::DownloadAckMsg ack;
    ack.code         = ec::kOk.code;
    ack.msg          = ec::kOk.msg;
    ack.download_url = file->file_path;
    ack.file_name    = file->file_name;
    ack.file_size    = file->file_size;
    SendPacket(conn, Cmd::kDownloadAck, seq, 0, ack);

    NOVA_NLOG_DEBUG(kLogTag, "download: user={}, file_id={}", user_id, req->file_id);
}

}  // namespace nova
