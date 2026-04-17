#include "file_service.h"
#include "errors/file_errors.h"
#include "../core/logger.h"
#include "../dao/file_dao.h"
#include "../dao/user_dao.h"

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "FileService";

void FileService::HandleUpdateAvatar(ConnectionPtr conn, Packet& pkt) {
    auto session = ctx_.dao().Session();
    uint32_t seq = pkt.seq;
    int64_t internal_id = conn->user_id();

    if (internal_id == 0) {
        SendPacket(conn, Cmd::kUpdateAvatarAck, seq, 0,
                   proto::UpdateAvatarAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    // 1. 反序列化
    auto req = proto::Deserialize<proto::UpdateAvatarReq>(pkt.body);
    if (!req) {
        SendPacket(conn, Cmd::kUpdateAvatarAck, seq, 0,
                   proto::UpdateAvatarAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    // 2. 校验 avatar_path
    if (req->avatar_path.empty()) {
        SendPacket(conn, Cmd::kUpdateAvatarAck, seq, 0,
                   proto::UpdateAvatarAck{ec::file::kAvatarPathEmpty.code, ec::file::kAvatarPathEmpty.msg});
        return;
    }
    if (req->avatar_path.size() > 512) {
        SendPacket(conn, Cmd::kUpdateAvatarAck, seq, 0,
                   proto::UpdateAvatarAck{ec::file::kAvatarPathTooLong.code, ec::file::kAvatarPathTooLong.msg});
        return;
    }

    // 3. 更新 users 表的 avatar 字段
    if (!ctx_.dao().User().UpdateAvatar(conn->uid(), req->avatar_path)) {
        SendPacket(conn, Cmd::kUpdateAvatarAck, seq, 0,
                   proto::UpdateAvatarAck{ec::file::kUpdateFailed.code, ec::file::kUpdateFailed.msg});
        return;
    }

    // 4. 在 user_files 表中记录文件元数据（旧头像标记删除）
    ctx_.dao().File().SoftDeleteByUserAndType(internal_id, "avatar");

    UserFile file;
    file.user_id   = internal_id;
    file.file_type = "avatar";
    file.file_path = req->avatar_path;
    file.hash      = req->file_hash;
    ctx_.dao().File().Insert(file);

    NOVA_NLOG_INFO(kLogTag, "user {} updated avatar: {}", conn->uid(), req->avatar_path);

    SendPacket(conn, Cmd::kUpdateAvatarAck, seq, 0,
               proto::UpdateAvatarAck{ec::kOk.code, ec::kOk.msg, req->avatar_path});
}

}  // namespace nova
