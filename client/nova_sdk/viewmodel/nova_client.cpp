#include "nova_client.h"

#include <core/client_config.h>
#include <core/client_context.h>
#include <service/auth_service.h>
#include <service/message_service.h>
#include <service/sync_service.h>
#include <service/profile_service.h>
#include <service/friend_service.h>
#include <service/conversation_service.h>
#include <service/group_service.h>
#include <service/file_service.h>

namespace nova::client {

// ================================================================
//  Impl — 内部实现（持有 ClientContext + 所有 Service）
// ================================================================

struct NovaClient::Impl {
    std::unique_ptr<ClientContext> ctx;

    // Services（生命周期由 Impl 管理，外部不可见）
    std::unique_ptr<AuthService>         auth;
    std::unique_ptr<MessageService>      msg;
    std::unique_ptr<SyncService>         sync;
    std::unique_ptr<ProfileService>      profile;
    std::unique_ptr<FriendService>       friend_svc;
    std::unique_ptr<ConversationService> conv;
    std::unique_ptr<GroupService>        group;
    std::unique_ptr<FileService>         file;

    // ViewModels（缓存单例）
    std::shared_ptr<AppVM>          app_vm;
    std::shared_ptr<LoginVM>        login_vm;
    std::shared_ptr<ChatVM>         chat_vm;
    std::shared_ptr<ContactVM>      contact_vm;
    std::shared_ptr<ConversationVM> conv_vm;
    std::shared_ptr<GroupVM>        group_vm;

    explicit Impl(const std::string& config_path) {
        ClientConfig config;
        LoadClientConfig(config, config_path);
        ctx = std::make_unique<ClientContext>(config);
        auth = std::make_unique<AuthService>(*ctx);
        msg = std::make_unique<MessageService>(*ctx);
        sync = std::make_unique<SyncService>(*ctx);
        profile = std::make_unique<ProfileService>(*ctx);
        friend_svc = std::make_unique<FriendService>(*ctx);
        conv = std::make_unique<ConversationService>(*ctx);
        group = std::make_unique<GroupService>(*ctx);
        file = std::make_unique<FileService>(*ctx);
    }

    void CreateVMs() {
        app_vm     = std::make_shared<AppVM>(*ctx);
        login_vm   = std::make_shared<LoginVM>(*auth);
        chat_vm    = std::make_shared<ChatVM>(*msg, *sync, *file);
        contact_vm = std::make_shared<ContactVM>(*friend_svc, *profile);
        conv_vm    = std::make_shared<ConversationVM>(*conv);
        group_vm   = std::make_shared<GroupVM>(*group);
    }
};

// ================================================================
//  构造 / 生命周期
// ================================================================

NovaClient::NovaClient(const std::string& config_path)
    : impl_(std::make_unique<Impl>(config_path)) {}

NovaClient::~NovaClient() {
    Shutdown();
}

void NovaClient::Init() {
    impl_->ctx->Init();
    impl_->CreateVMs();
}

void NovaClient::Shutdown() {
    if (impl_ && impl_->ctx) impl_->ctx->Shutdown();
}

// ================================================================
//  连接
// ================================================================

void NovaClient::Connect() {
    impl_->ctx->Connect();
}

void NovaClient::Disconnect() {
    impl_->ctx->Network().Disconnect();
}

// ================================================================
//  ViewModel 访问
// ================================================================

std::shared_ptr<AppVM> NovaClient::App() {
    return impl_->app_vm;
}

std::shared_ptr<LoginVM> NovaClient::Login() {
    return impl_->login_vm;
}

std::shared_ptr<ChatVM> NovaClient::Chat() {
    return impl_->chat_vm;
}

std::shared_ptr<ContactVM> NovaClient::Contacts() {
    return impl_->contact_vm;
}

std::shared_ptr<ConversationVM> NovaClient::Conversations() {
    return impl_->conv_vm;
}

std::shared_ptr<GroupVM> NovaClient::Groups() {
    return impl_->group_vm;
}

}  // namespace nova::client
