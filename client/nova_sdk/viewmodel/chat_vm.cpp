#include "chat_vm.h"
#include <service/message_service.h>
#include <service/sync_service.h>
#include <service/file_service.h>

namespace nova::client {

ChatVM::ChatVM(MessageService& msg, SyncService& sync, FileService& file)
    : msg_(msg), sync_(sync), file_(file) {}
ChatVM::~ChatVM() = default;

// ---- 消息 ----

void ChatVM::SendTextMessage(int64_t conversation_id, const std::string& content,
                             SendMsgCallback cb) {
    msg_.SendTextMessage(conversation_id, content, std::move(cb));
}

void ChatVM::RecallMessage(int64_t conversation_id, int64_t server_seq, ResultCallback cb) {
    msg_.RecallMessage(conversation_id, server_seq, std::move(cb));
}

void ChatVM::SendDeliverAck(int64_t conversation_id, int64_t server_seq) {
    msg_.SendDeliverAck(conversation_id, server_seq);
}

void ChatVM::SendReadAck(int64_t conversation_id, int64_t read_up_to_seq) {
    msg_.SendReadAck(conversation_id, read_up_to_seq);
}

// ---- 同步 ----

void ChatVM::SyncMessages(int64_t conversation_id, int64_t last_seq, int32_t limit,
                          SyncMsgCallback cb) {
    sync_.SyncMessages(conversation_id, last_seq, limit, std::move(cb));
}

void ChatVM::SyncUnread(SyncUnreadCallback cb) {
    sync_.SyncUnread(std::move(cb));
}

// ---- 文件 ----

void ChatVM::RequestUpload(const std::string& file_name, int64_t file_size,
                           const std::string& mime_type, const std::string& file_hash,
                           const std::string& file_type, UploadCallback cb) {
    file_.RequestUpload(file_name, file_size, mime_type, file_hash, file_type, std::move(cb));
}

void ChatVM::UploadComplete(int64_t file_id, UploadCompleteCallback cb) {
    file_.UploadComplete(file_id, std::move(cb));
}

void ChatVM::RequestDownload(int64_t file_id, bool thumb, DownloadCallback cb) {
    file_.RequestDownload(file_id, thumb, std::move(cb));
}

// ---- 事件监听 ----

void ChatVM::OnMessageReceived(MessageCallback cb) {
    msg_.OnReceived(std::move(cb));
}

void ChatVM::OnMessageRecalled(RecallCallback cb) {
    msg_.OnRecalled(std::move(cb));
}

}  // namespace nova::client
