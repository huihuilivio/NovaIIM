#pragma once

#include "service_base.h"

namespace nova {

class FileService : public ServiceBase {
public:
    explicit FileService(ServerContext& ctx) : ServiceBase(ctx) {}

    void HandleUpload(ConnectionPtr conn, Packet& pkt);
    void HandleUploadComplete(ConnectionPtr conn, Packet& pkt);
    void HandleDownload(ConnectionPtr conn, Packet& pkt);
};

}  // namespace nova
