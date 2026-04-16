#pragma once

#include "service_base.h"

namespace nova {

// 文件服务
// 职责：头像更新、用户资料查询，后续扩展文件上传/下载元数据管理
class FileService : public ServiceBase {
public:
    explicit FileService(ServerContext& ctx) : ServiceBase(ctx) {}

    void HandleUpdateAvatar(ConnectionPtr conn, Packet& pkt);
    void HandleGetUserProfile(ConnectionPtr conn, Packet& pkt);
};

}  // namespace nova
