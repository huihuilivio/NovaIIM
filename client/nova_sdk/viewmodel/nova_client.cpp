#include "nova_client.h"

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

    explicit Impl(const ClientConfig& config)
        : ctx(std::make_unique<ClientContext>(config)),
          auth(std::make_unique<AuthService>(*ctx)),
          msg(std::make_unique<MessageService>(*ctx)),
          sync(std::make_unique<SyncService>(*ctx)),
          profile(std::make_unique<ProfileService>(*ctx)),
          friend_svc(std::make_unique<FriendService>(*ctx)),
          conv(std::make_unique<ConversationService>(*ctx)),
          group(std::make_unique<GroupService>(*ctx)),
          file(std::make_unique<FileService>(*ctx)) {}
};

// ================================================================
//  构造 / 生命周期
// ================================================================

NovaClient::NovaClient(const ClientConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

NovaClient::~NovaClient() {
    Shutdown();
}

void NovaClient::Init() {
    impl_->ctx->Init();
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

const ClientConfig& NovaClient::Config() const {
    return impl_->ctx->Config();
}

// ================================================================
//  ViewModel 工厂方法
// ================================================================

std::shared_ptr<AppVM> NovaClient::App() {
    return std::make_shared<AppVM>(*impl_->ctx);
}

std::shared_ptr<LoginVM> NovaClient::Login() {
    return std::make_shared<LoginVM>(*impl_->auth);
}

std::shared_ptr<ChatVM> NovaClient::Chat() {
    return std::make_shared<ChatVM>(*impl_->msg, *impl_->sync, *impl_->file);
}

std::shared_ptr<ContactVM> NovaClient::Contacts() {
    return std::make_shared<ContactVM>(*impl_->friend_svc, *impl_->profile);
}

std::shared_ptr<ConversationVM> NovaClient::Conversations() {
    return std::make_shared<ConversationVM>(*impl_->conv);
}

std::shared_ptr<GroupVM> NovaClient::Groups() {
    return std::make_shared<GroupVM>(*impl_->group);
}

}  // namespace nova::client
