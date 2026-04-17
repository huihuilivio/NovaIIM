#pragma once

#include "service_base.h"

namespace nova {

// 文件服务
// 职责：后续扩展文件上传/下载元数据管理
// 头像更新已统一到 UserService::HandleUpdateProfile
class FileService : public ServiceBase {
public:
    explicit FileService(ServerContext& ctx) : ServiceBase(ctx) {}
};

}  // namespace nova
