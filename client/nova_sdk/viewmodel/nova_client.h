#pragma once
// NovaClient — SDK 公共入口（Facade）
//
// 平台代码（Desktop / iOS / Android）统一通过 NovaClient 访问各 ViewModel
// Service 层内部不可见，ViewModel 层对应 UI 界面
//
// 用法:
//   nova::client::NovaClient client(config);
//   client.Init();
//   client.Connect();
//   client.Login()->Login("user@email.com", "password", callback);
//   client.Chat()->SendTextMessage(conv_id, "hello", callback);

#include <export.h>
#include <model/client_config.h>

#include <viewmodel/app_vm.h>
#include <viewmodel/login_vm.h>
#include <viewmodel/chat_vm.h>
#include <viewmodel/contact_vm.h>
#include <viewmodel/conversation_vm.h>
#include <viewmodel/group_vm.h>

#include <memory>

namespace nova::client {

class NOVA_SDK_API NovaClient {
public:
    // ================================================================
    //  构造 / 生命周期
    // ================================================================

    explicit NovaClient(const ClientConfig& config);
    ~NovaClient();

    NovaClient(const NovaClient&) = delete;
    NovaClient& operator=(const NovaClient&) = delete;

    void Init();
    void Shutdown();

    // ================================================================
    //  连接
    // ================================================================

    void Connect();
    void Disconnect();

    // ================================================================
    //  配置
    // ================================================================

    const ClientConfig& Config() const;

    // ================================================================
    //  ViewModel 工厂方法（每次调用创建新实例）
    // ================================================================

    std::shared_ptr<AppVM>          App();
    std::shared_ptr<LoginVM>        Login();
    std::shared_ptr<ChatVM>         Chat();
    std::shared_ptr<ContactVM>      Contacts();
    std::shared_ptr<ConversationVM> Conversations();
    std::shared_ptr<GroupVM>        Groups();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nova::client
