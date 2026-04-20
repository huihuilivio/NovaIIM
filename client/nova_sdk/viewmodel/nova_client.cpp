#include "nova_client.h"
#include <model/client_context.h>
#include <infra/logger.h>

#include <nova/packet.h>
#include <nova/protocol.h>

namespace nova::client {

NovaClient::NovaClient(const ClientConfig& config)
    : ctx_(std::make_unique<ClientContext>(config)) {}

NovaClient::~NovaClient() {
    Shutdown();
}

void NovaClient::Init() {
    ctx_->Init();
}

void NovaClient::Shutdown() {
    if (ctx_) ctx_->Shutdown();
}

void NovaClient::Connect() {
    ctx_->Connect();
}

void NovaClient::Disconnect() {
    ctx_->Network().Disconnect();
}

ClientState NovaClient::GetState() const {
    return ctx_->GetState();
}

void NovaClient::Login(const std::string& email, const std::string& password, LoginCallback cb) {
    nova::proto::LoginReq req;
    req.email       = email;
    req.password    = password;
    req.device_id   = ctx_->Config().device_id;
    req.device_type = ctx_->Config().device_type;

    nova::proto::Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(nova::proto::Cmd::kLogin);
    pkt.seq  = ctx_->NextSeq();
    pkt.body = nova::proto::Serialize(req);

    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    ctx_->Requests().AddPending(pkt.seq,
        [this, cb](const nova::proto::Packet& resp) {
            auto ack = nova::proto::Deserialize<nova::proto::LoginAck>(resp.body);
            LoginResult result;
            if (ack && ack->code == 0) {
                ctx_->SetAuthenticated(ack->uid);
                result.success  = true;
                result.uid      = ack->uid;
                result.nickname = ack->nickname;
                result.avatar   = ack->avatar;
            } else {
                result.msg = ack ? ack->msg : "deserialize error";
            }
            if (cb) cb(result);
        },
        [cb](uint32_t /*seq*/) {
            if (cb) cb({.success = false, .msg = "login timeout"});
        }
    );

    ctx_->SendPacket(pkt);
}

void NovaClient::Logout() {
    nova::proto::Packet pkt;
    pkt.cmd = static_cast<uint16_t>(nova::proto::Cmd::kLogout);
    pkt.seq = ctx_->NextSeq();
    ctx_->SendPacket(pkt);
    ctx_->Shutdown();
}

bool NovaClient::IsLoggedIn() const {
    return ctx_->IsLoggedIn();
}

const std::string& NovaClient::Uid() const {
    return ctx_->Uid();
}

void NovaClient::SendTextMessage(int64_t conversation_id, const std::string& content, SendMsgCallback cb) {
    nova::proto::SendMsgReq req;
    req.conversation_id = conversation_id;
    req.content         = content;
    req.msg_type        = nova::proto::MsgType::kText;

    nova::proto::Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(nova::proto::Cmd::kSendMsg);
    pkt.seq  = ctx_->NextSeq();
    pkt.body = nova::proto::Serialize(req);

    if (pkt.body.empty()) {
        if (cb) cb({.success = false, .msg = "serialize error"});
        return;
    }

    ctx_->Requests().AddPending(pkt.seq,
        [cb](const nova::proto::Packet& resp) {
            auto ack = nova::proto::Deserialize<nova::proto::SendMsgAck>(resp.body);
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
        [cb](uint32_t /*seq*/) {
            if (cb) cb({.success = false, .msg = "send timeout"});
        }
    );

    ctx_->SendPacket(pkt);
}

void NovaClient::OnStateChanged(StateCallback cb) {
    ctx_->OnStateChanged(std::move(cb));
}

void NovaClient::OnMessageReceived(MessageCallback cb) {
    ctx_->Events().subscribe<nova::proto::PushMsg>("PushMsg",
        [cb](const nova::proto::PushMsg& msg) {
            ReceivedMessage rm;
            rm.conversation_id = msg.conversation_id;
            rm.sender_uid      = msg.sender_uid;
            rm.content         = msg.content;
            rm.server_seq      = msg.server_seq;
            rm.server_time     = msg.server_time;
            rm.msg_type        = static_cast<int>(msg.msg_type);
            cb(rm);
        });
}

void NovaClient::OnMessageRecalled(RecallCallback cb) {
    ctx_->Events().subscribe<nova::proto::RecallNotify>("RecallNotify",
        [cb](const nova::proto::RecallNotify& n) {
            RecallNotification rn;
            rn.conversation_id = n.conversation_id;
            rn.server_seq      = n.server_seq;
            rn.operator_uid    = n.operator_uid;
            cb(rn);
        });
}

const ClientConfig& NovaClient::Config() const {
    return ctx_->Config();
}

}  // namespace nova::client
