#include "conversation_vm.h"
#include <service/conversation_service.h>

namespace nova::client {

ConversationVM::ConversationVM(ConversationService& conv) : conv_(conv) {}
ConversationVM::~ConversationVM() = default;

void ConversationVM::GetConversationList(ConvListCallback cb) {
    conv_.GetConversationList(std::move(cb));
}

void ConversationVM::DeleteConversation(int64_t conversation_id, ResultCallback cb) {
    conv_.DeleteConversation(conversation_id, std::move(cb));
}

void ConversationVM::MuteConversation(int64_t conversation_id, bool mute, ResultCallback cb) {
    conv_.MuteConversation(conversation_id, mute, std::move(cb));
}

void ConversationVM::PinConversation(int64_t conversation_id, bool pinned, ResultCallback cb) {
    conv_.PinConversation(conversation_id, pinned, std::move(cb));
}

void ConversationVM::OnUpdated(ConvNotifyCallback cb) {
    conv_.OnUpdated(std::move(cb));
}

}  // namespace nova::client
