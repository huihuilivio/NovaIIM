#include "conv_service.h"
#include "../core/logger.h"
#include "../dao/conversation_dao.h"
#include "../dao/message_dao.h"
#include "../dao/user_dao.h"
#include <nova/errors.h>

#include <unordered_map>

namespace nova {

namespace ec = errc;

static constexpr const char* kLogTag = "ConvSvc";

// ---- ConvUpdate 推送辅助 ----

void BroadcastConvUpdate(ServerContext& ctx, int64_t conversation_id,
                         int64_t exclude_user_id, const proto::ConvUpdateMsg& update) {
    auto members = ctx.dao().Conversation().GetMembersByConversation(conversation_id);
    Packet pkt;
    pkt.cmd  = static_cast<uint16_t>(Cmd::kConvUpdate);
    pkt.seq  = 0;
    pkt.uid  = 0;
    pkt.body = proto::Serialize(update);

    for (const auto& m : members) {
        if (m.user_id == exclude_user_id)
            continue;
        auto conns = ctx.conn_manager().GetConns(m.user_id);
        for (auto& c : conns) {
            c->Send(pkt);
        }
    }
}

// ---- handlers ----

void ConvService::HandleGetConvList(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kGetConvListAck, seq, 0,
                   proto::GetConvListAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    // 1. 查询用户参与的所有会话成员关系
    auto memberships = ctx_.dao().Conversation().GetMembersByUser(user_id);

    // 过滤已隐藏的会话
    std::vector<ConversationMember> visible;
    visible.reserve(memberships.size());
    for (auto& m : memberships) {
        if (m.hidden == 0) {
            visible.push_back(std::move(m));
        }
    }

    // 2. 批量获取所有会话信息
    std::vector<int64_t> conv_ids;
    conv_ids.reserve(visible.size());
    for (const auto& m : visible) {
        conv_ids.push_back(m.conversation_id);
    }
    auto conversations = ctx_.dao().Conversation().FindByIds(conv_ids);

    std::unordered_map<int64_t, const Conversation*> conv_map;
    conv_map.reserve(conversations.size());
    for (const auto& c : conversations) {
        conv_map[c.id] = &c;
    }

    // 3. 批量获取最新一条消息用于预览
    std::vector<std::pair<int64_t, int64_t>> preview_convs;
    preview_convs.reserve(visible.size());
    for (const auto& m : visible) {
        auto it = conv_map.find(m.conversation_id);
        if (it == conv_map.end())
            continue;
        int64_t from_seq = std::max<int64_t>(0, it->second->max_seq - 1);
        preview_convs.emplace_back(m.conversation_id, from_seq);
    }

    std::unordered_map<int64_t, const Message*> latest_msg_map;
    std::vector<Message> all_previews;
    if (!preview_convs.empty()) {
        all_previews = ctx_.dao().Message().GetLatestByConversations(preview_convs, 1);
        for (const auto& m : all_previews) {
            // 取每个会话的最后一条（最高 seq）
            auto it = latest_msg_map.find(m.conversation_id);
            if (it == latest_msg_map.end() || m.seq > it->second->seq) {
                latest_msg_map[m.conversation_id] = &m;
            }
        }
    }

    // 4. sender_uid + nickname 缓存（避免 N+1）+ 私聊对方映射
    std::unordered_map<int64_t, const User*> user_cache;
    std::vector<User> cached_users;
    std::vector<int64_t> needed_ids;
    std::unordered_map<int64_t, int64_t> private_other_map;

    for (const auto& m : visible) {
        auto it = conv_map.find(m.conversation_id);
        if (it == conv_map.end())
            continue;
        const auto& conv = *(it->second);
        if (conv.type == static_cast<int>(ConvType::kPrivate)) {
            auto members = ctx_.dao().Conversation().GetMembersByConversation(m.conversation_id);
            for (const auto& member : members) {
                if (member.user_id != user_id) {
                    needed_ids.push_back(member.user_id);
                    private_other_map[m.conversation_id] = member.user_id;
                    break;
                }
            }
        }
    }
    for (const auto& [cid, msg] : latest_msg_map) {
        needed_ids.push_back(msg->sender_id);
    }

    if (!needed_ids.empty()) {
        cached_users = ctx_.dao().User().FindByIds(needed_ids);
        for (const auto& u : cached_users) {
            user_cache[u.id] = &u;
        }
    }

    // 5. 构建响应
    proto::GetConvListAck ack;
    ack.code = ec::kOk.code;
    ack.msg  = ec::kOk.msg;
    ack.conversations.reserve(visible.size());

    for (const auto& m : visible) {
        auto it = conv_map.find(m.conversation_id);
        if (it == conv_map.end())
            continue;
        const auto& conv = *(it->second);

        proto::ConvItem item;
        item.conversation_id = conv.id;
        item.type            = conv.type;
        item.mute            = m.mute;
        item.pinned          = m.pinned;
        item.updated_at      = conv.updated_at;

        // 名称和头像：私聊用对方信息，群聊用会话信息
        if (conv.type == static_cast<int>(ConvType::kPrivate)) {
            auto oit = private_other_map.find(conv.id);
            if (oit != private_other_map.end()) {
                auto uit = user_cache.find(oit->second);
                if (uit != user_cache.end()) {
                    item.name   = uit->second->nickname;
                    item.avatar = uit->second->avatar;
                }
            }
        } else {
            item.name   = conv.name;
            item.avatar = conv.avatar;
        }

        // 未读数
        int64_t unread = conv.max_seq - m.last_read_seq;
        if (unread < 0) unread = 0;
        item.unread_count = unread;

        // 最后消息摘要
        auto msg_it = latest_msg_map.find(conv.id);
        if (msg_it != latest_msg_map.end()) {
            const auto* msg = msg_it->second;
            auto uit = user_cache.find(msg->sender_id);
            item.last_msg.sender_uid      = uit != user_cache.end() ? uit->second->uid : "";
            item.last_msg.sender_nickname = uit != user_cache.end() ? uit->second->nickname : "";
            item.last_msg.content         = msg->content.size() > 100 ? msg->content.substr(0, 100) : msg->content;
            item.last_msg.msg_type        = msg->msg_type;
            // server_time: 使用 created_at 的 epoch ms（简化处理，返回 0 让客户端用时间字符串）
            item.last_msg.server_time     = 0;
        }

        ack.conversations.push_back(std::move(item));
    }

    SendPacket(conn, Cmd::kGetConvListAck, seq, 0, ack);

    NOVA_NLOG_DEBUG(kLogTag, "get_conv_list: user={}, count={}", user_id, ack.conversations.size());
}

void ConvService::HandleDeleteConv(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kDeleteConvAck, seq, 0,
                   proto::DeleteConvAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::DeleteConvReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kDeleteConvAck, seq, 0,
                   proto::DeleteConvAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (!ctx_.dao().Conversation().IsMember(req->conversation_id, user_id)) {
        SendPacket(conn, Cmd::kDeleteConvAck, seq, 0,
                   proto::DeleteConvAck{ec::conv::kNotMember.code, ec::conv::kNotMember.msg});
        return;
    }

    // 标记隐藏（不删除数据，收到新消息自动恢复）
    ctx_.dao().Conversation().UpdateMemberHidden(req->conversation_id, user_id, 1);

    NOVA_NLOG_INFO(kLogTag, "conv hidden: user={}, conv={}", user_id, req->conversation_id);
    SendPacket(conn, Cmd::kDeleteConvAck, seq, 0,
               proto::DeleteConvAck{ec::kOk.code, ec::kOk.msg});
}

void ConvService::HandleMuteConv(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kMuteConvAck, seq, 0,
                   proto::MuteConvAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::MuteConvReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kMuteConvAck, seq, 0,
                   proto::MuteConvAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->mute != 0 && req->mute != 1) {
        SendPacket(conn, Cmd::kMuteConvAck, seq, 0,
                   proto::MuteConvAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto member = ctx_.dao().Conversation().FindMember(req->conversation_id, user_id);
    if (!member) {
        SendPacket(conn, Cmd::kMuteConvAck, seq, 0,
                   proto::MuteConvAck{ec::conv::kNotMember.code, ec::conv::kNotMember.msg});
        return;
    }

    ctx_.dao().Conversation().UpdateMemberMute(req->conversation_id, user_id, req->mute);

    NOVA_NLOG_INFO(kLogTag, "conv mute: user={}, conv={}, mute={}", user_id, req->conversation_id, req->mute);
    SendPacket(conn, Cmd::kMuteConvAck, seq, 0,
               proto::MuteConvAck{ec::kOk.code, ec::kOk.msg});
}

void ConvService::HandlePinConv(ConnectionPtr conn, Packet& pkt) {
    auto session    = ctx_.dao().Session();
    int64_t user_id = conn->user_id();
    uint32_t seq    = pkt.seq;

    if (user_id == 0) {
        SendPacket(conn, Cmd::kPinConvAck, seq, 0,
                   proto::PinConvAck{ec::kNotAuthenticated.code, ec::kNotAuthenticated.msg});
        return;
    }

    auto req = proto::Deserialize<proto::PinConvReq>(pkt.body);
    if (!req || req->conversation_id <= 0) {
        SendPacket(conn, Cmd::kPinConvAck, seq, 0,
                   proto::PinConvAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    if (req->pinned != 0 && req->pinned != 1) {
        SendPacket(conn, Cmd::kPinConvAck, seq, 0,
                   proto::PinConvAck{ec::kInvalidBody.code, ec::kInvalidBody.msg});
        return;
    }

    auto member = ctx_.dao().Conversation().FindMember(req->conversation_id, user_id);
    if (!member) {
        SendPacket(conn, Cmd::kPinConvAck, seq, 0,
                   proto::PinConvAck{ec::conv::kNotMember.code, ec::conv::kNotMember.msg});
        return;
    }

    ctx_.dao().Conversation().UpdateMemberPinned(req->conversation_id, user_id, req->pinned);

    NOVA_NLOG_INFO(kLogTag, "conv pin: user={}, conv={}, pinned={}", user_id, req->conversation_id, req->pinned);
    SendPacket(conn, Cmd::kPinConvAck, seq, 0,
               proto::PinConvAck{ec::kOk.code, ec::kOk.msg});
}

}  // namespace nova
