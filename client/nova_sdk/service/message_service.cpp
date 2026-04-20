#include "message_service.h"
#include "service_helpers.h"

namespace nova::client {

using namespace detail;

MessageService::MessageService(ClientContext& ctx) : ctx_(ctx) {}

void MessageService::SendTextMessage(int64_t conversation_id, const std::string& content,
                                SendMsgCallback cb) {
    nova::proto::SendMsgReq req;
    req.conversation_id = conversation_id;
    req.content         = content;
    req.msg_type        = nova::proto::MsgType::kText;

    auto pkt = MakePacket(nova::proto::Cmd::kSendMsg, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::SendMsgAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::SendMsgAck>& ack) {
            SendMsgResult result;
            if (ack && ack->code == 0) {
                result.success     = true;
                result.server_seq  = ack->server_seq;
                result.server_time = ack->server_time;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb]() { if (cb) cb({.success = false, .msg = "send timeout"}); }
    );
}

void MessageService::RecallMessage(int64_t conversation_id, int64_t server_seq, ResultCallback cb) {
    nova::proto::RecallMsgReq req;
    req.conversation_id = conversation_id;
    req.server_seq      = server_seq;

    auto pkt = MakePacket(nova::proto::Cmd::kRecallMsg, ctx_.NextSeq(), req);
    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    SendRequest<nova::proto::RecallMsgAck>(ctx_, pkt,
        [cb](const std::optional<nova::proto::RecallMsgAck>& ack) {
            if (!ack) { if (cb) cb({.success = false, .msg = "deserialize error"}); return; }
            if (cb) cb({.success = (ack->code == 0), .msg = ack->msg});
        },
        [cb]() { if (cb) cb({.success = false, .msg = "recall timeout"}); }
    );
}

void MessageService::SendDeliverAck(int64_t conversation_id, int64_t server_seq) {
    nova::proto::DeliverAckReq req;
    req.conversation_id = conversation_id;
    req.server_seq      = server_seq;

    auto pkt = MakePacket(nova::proto::Cmd::kDeliverAck, ctx_.NextSeq(), req);
    ctx_.SendPacket(pkt);
}

void MessageService::SendReadAck(int64_t conversation_id, int64_t read_up_to_seq) {
    nova::proto::ReadAckReq req;
    req.conversation_id = conversation_id;
    req.read_up_to_seq  = read_up_to_seq;

    auto pkt = MakePacket(nova::proto::Cmd::kReadAck, ctx_.NextSeq(), req);
    ctx_.SendPacket(pkt);
}

void MessageService::OnReceived(MessageCallback cb) {
    ctx_.Events().subscribe<nova::proto::PushMsg>("PushMsg",
        [cb](const nova::proto::PushMsg& msg) {
            cb({
                .conversation_id = msg.conversation_id,
                .sender_uid      = msg.sender_uid,
                .content         = msg.content,
                .server_seq      = msg.server_seq,
                .server_time     = msg.server_time,
                .msg_type        = static_cast<int>(msg.msg_type),
            });
        });
}

void MessageService::OnRecalled(RecallCallback cb) {
    ctx_.Events().subscribe<nova::proto::RecallNotify>("RecallNotify",
        [cb](const nova::proto::RecallNotify& n) {
            cb({
                .conversation_id = n.conversation_id,
                .server_seq      = n.server_seq,
                .operator_uid    = n.operator_uid,
            });
        });
}

}  // namespace nova::client
