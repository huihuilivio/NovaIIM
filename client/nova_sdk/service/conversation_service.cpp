#include "conversation_service.h"
#include "service_helpers.h"

namespace nova::client {

using namespace detail;

ConversationService::ConversationService(ClientContext& ctx) : ctx_(ctx) {}

void ConversationService::GetConversationList(ConvListCallback cb) {
    nova::proto::GetConvListReq req;

    auto pkt = MakePacket(nova::proto::Cmd::kGetConvList, ctx_.NextSeq(), req);

    SendRequest<nova::proto::GetConvListAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::GetConvListAck>& ack) {
            ConvListResult result;
            if (ack && ack->code == 0) {
                result.success = true;
                result.conversations.reserve(ack->conversations.size());
                for (const auto& c : ack->conversations) {
                    result.conversations.push_back({
                        .conversation_id = c.conversation_id,
                        .type            = c.type,
                        .name            = c.name,
                        .avatar          = c.avatar,
                        .unread_count    = c.unread_count,
                        .last_msg = {
                            .sender_uid      = c.last_msg.sender_uid,
                            .sender_nickname = c.last_msg.sender_nickname,
                            .content         = c.last_msg.content,
                            .msg_type        = static_cast<int>(c.last_msg.msg_type),
                            .server_time     = c.last_msg.server_time,
                        },
                        .mute       = c.mute,
                        .pinned     = c.pinned,
                        .updated_at = c.updated_at,
                    });
                }
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void ConversationService::DeleteConversation(int64_t conversation_id, ResultCallback cb) {
    nova::proto::DeleteConvReq req;
    req.conversation_id = conversation_id;

    auto pkt = MakePacket(nova::proto::Cmd::kDeleteConv, ctx_.NextSeq(), req);

    SendRequest<nova::proto::DeleteConvAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::DeleteConvAck>& ack) {
            if (!ack) { if (cb) cb({.success = false, .msg = "deserialize error"}); return; }
            if (cb) cb({.success = (ack->code == 0), .msg = ack->msg});
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void ConversationService::MuteConversation(int64_t conversation_id, bool mute, ResultCallback cb) {
    nova::proto::MuteConvReq req;
    req.conversation_id = conversation_id;
    req.mute            = mute ? 1 : 0;

    auto pkt = MakePacket(nova::proto::Cmd::kMuteConv, ctx_.NextSeq(), req);

    SendRequest<nova::proto::MuteConvAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::MuteConvAck>& ack) {
            if (!ack) { if (cb) cb({.success = false, .msg = "deserialize error"}); return; }
            if (cb) cb({.success = (ack->code == 0), .msg = ack->msg});
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void ConversationService::PinConversation(int64_t conversation_id, bool pinned, ResultCallback cb) {
    nova::proto::PinConvReq req;
    req.conversation_id = conversation_id;
    req.pinned          = pinned ? 1 : 0;

    auto pkt = MakePacket(nova::proto::Cmd::kPinConv, ctx_.NextSeq(), req);

    SendRequest<nova::proto::PinConvAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::PinConvAck>& ack) {
            if (!ack) { if (cb) cb({.success = false, .msg = "deserialize error"}); return; }
            if (cb) cb({.success = (ack->code == 0), .msg = ack->msg});
        },
        [cb]() { if (cb) cb({.success = false, .msg = "timeout"}); }
    );
}

void ConversationService::OnUpdated(ConvNotifyCallback cb) {
    ctx_.Events().subscribe<nova::proto::ConvUpdateMsg>("ConvUpdate",
        [cb](const nova::proto::ConvUpdateMsg& n) {
            cb({
                .conversation_id = n.conversation_id,
                .update_type     = n.update_type,
                .data            = n.data,
            });
        });
}

}  // namespace nova::client
