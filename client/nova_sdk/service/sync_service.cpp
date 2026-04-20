#include "sync_service.h"
#include "service_helpers.h"

namespace nova::client {

using namespace detail;

SyncService::SyncService(ClientContext& ctx) : ctx_(ctx) {}

void SyncService::SyncMessages(int64_t conversation_id, int64_t last_seq, int32_t limit,
                          SyncMsgCallback cb) {
    nova::proto::SyncMsgReq req;
    req.conversation_id = conversation_id;
    req.last_seq        = last_seq;
    req.limit           = limit;

    auto pkt = MakePacket(nova::proto::Cmd::kSyncMsg, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false});
        return;
    }

    SendRequest<nova::proto::SyncMsgResp>(ctx_, pkt,
        [cb](const std::optional<nova::proto::SyncMsgResp>& ack) {
            SyncMsgResult result;
            if (ack && ack->code == 0) {
                result.success  = true;
                result.has_more = ack->has_more;
                result.messages.reserve(ack->messages.size());
                for (const auto& m : ack->messages) {
                    result.messages.push_back({
                        .server_seq  = m.server_seq,
                        .sender_uid  = m.sender_uid,
                        .content     = m.content,
                        .msg_type    = static_cast<int>(m.msg_type),
                        .server_time = m.server_time,
                        .status      = static_cast<int>(m.status),
                    });
                }
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false}); }
    );
}

void SyncService::SyncUnread(SyncUnreadCallback cb) {
    auto pkt = MakePacket(nova::proto::Cmd::kSyncUnread, ctx_.NextSeq());

    SendRequest<nova::proto::SyncUnreadResp>(ctx_, pkt,
        [cb](const std::optional<nova::proto::SyncUnreadResp>& ack) {
            SyncUnreadResult result;
            if (ack && ack->code == 0) {
                result.success      = true;
                result.total_unread = ack->total_unread;
                result.items.reserve(ack->items.size());
                for (const auto& item : ack->items) {
                    UnreadEntry entry;
                    entry.conversation_id = item.conversation_id;
                    entry.count           = item.count;
                    entry.latest_messages.reserve(item.latest_messages.size());
                    for (const auto& m : item.latest_messages) {
                        entry.latest_messages.push_back({
                            .server_seq  = m.server_seq,
                            .sender_uid  = m.sender_uid,
                            .content     = m.content,
                            .msg_type    = static_cast<int>(m.msg_type),
                            .server_time = m.server_time,
                            .status      = static_cast<int>(m.status),
                        });
                    }
                    result.items.push_back(std::move(entry));
                }
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false}); }
    );
}

}  // namespace nova::client
