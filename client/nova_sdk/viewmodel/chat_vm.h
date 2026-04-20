#pragma once
// ChatVM — 聊天 ViewModel（消息收发 + 同步 + 文件）

#include <export.h>
#include <viewmodel/observable.h>
#include <viewmodel/types.h>

#include <functional>
#include <string>

namespace nova::client {

class MessageService;
class SyncService;
class FileService;

class NOVA_SDK_API ChatVM {
public:
    // Message callbacks
    using SendMsgCallback  = std::function<void(const SendMsgResult&)>;
    using MessageCallback  = std::function<void(const ReceivedMessage&)>;
    using RecallCallback   = std::function<void(const RecallNotification&)>;
    // Sync callbacks
    using SyncMsgCallback    = std::function<void(const SyncMsgResult&)>;
    using SyncUnreadCallback = std::function<void(const SyncUnreadResult&)>;
    // File callbacks
    using UploadCallback         = std::function<void(const UploadResult&)>;
    using UploadCompleteCallback = std::function<void(const UploadCompleteResult&)>;
    using DownloadCallback       = std::function<void(const DownloadResult&)>;

    ChatVM(MessageService& msg, SyncService& sync, FileService& file);
    ~ChatVM();

    // ---- 消息 ----
    void SendTextMessage(int64_t conversation_id, const std::string& content,
                         SendMsgCallback cb = nullptr);
    void RecallMessage(int64_t conversation_id, int64_t server_seq,
                       ResultCallback cb = nullptr);
    void SendDeliverAck(int64_t conversation_id, int64_t server_seq);
    void SendReadAck(int64_t conversation_id, int64_t read_up_to_seq);

    // ---- 同步 ----
    void SyncMessages(int64_t conversation_id, int64_t last_seq, int32_t limit,
                      SyncMsgCallback cb);
    void SyncUnread(SyncUnreadCallback cb);

    // ---- 文件 ----
    void RequestUpload(const std::string& file_name, int64_t file_size,
                       const std::string& mime_type, const std::string& file_hash,
                       const std::string& file_type, UploadCallback cb);
    void UploadComplete(int64_t file_id, UploadCompleteCallback cb);
    void RequestDownload(int64_t file_id, bool thumb, DownloadCallback cb);

    // ---- 事件监听 ----
    void OnMessageReceived(MessageCallback cb);
    void OnMessageRecalled(RecallCallback cb);

    /// 最近收到的消息列表（Observable）
    Observable<std::vector<ReceivedMessage>>& Messages() { return messages_; }

private:
    MessageService& msg_;
    SyncService& sync_;
    FileService& file_;
    Observable<std::vector<ReceivedMessage>> messages_;
};

}  // namespace nova::client
