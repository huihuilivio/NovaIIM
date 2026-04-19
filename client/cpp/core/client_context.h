#pragma once
// ClientContext — 客户端全局上下文（依赖注入容器）
//
// 持有 TcpClient / RequestManager / ReconnectManager 的生命周期
// 供 ViewModel 层使用

#include <core/export.h>
#include <core/client_config.h>
#include <core/event_bus.h>
#include <net/tcp_client.h>
#include <net/reconnect_manager.h>
#include <net/request_manager.h>

#include <memory>
#include <string>

namespace nova::client {

class NOVA_CLIENT_API ClientContext {
public:
    explicit ClientContext(const ClientConfig& config);
    ~ClientContext();

    ClientContext(const ClientContext&) = delete;
    ClientContext& operator=(const ClientContext&) = delete;

    /// 初始化所有子系统
    void Init();

    /// 关闭所有子系统
    void Shutdown();

    // ---- 访问器 ----
    TcpClient&        Network()    { return *tcp_client_; }
    RequestManager&   Requests()   { return *request_mgr_; }
    ReconnectManager& Reconnect()  { return *reconnect_mgr_; }
    EventBus&         Events()     { return EventBus::Get(); }
    const ClientConfig& Config() const { return config_; }

    // ---- 登录状态 ----
    void SetUid(const std::string& uid) { uid_ = uid; }
    const std::string& Uid() const { return uid_; }
    bool IsLoggedIn() const { return !uid_.empty(); }

private:
    void SetupPacketDispatch();

    ClientConfig config_;
    std::unique_ptr<TcpClient> tcp_client_;
    std::unique_ptr<RequestManager> request_mgr_;
    std::unique_ptr<ReconnectManager> reconnect_mgr_;
    std::string uid_;
};

}  // namespace nova::client
