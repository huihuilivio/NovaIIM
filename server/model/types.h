#pragma once

#include <cstdint>
#include <string>

namespace nova {

struct User {
    int64_t  id = 0;
    std::string uid;
    std::string nickname;
    std::string avatar;
    int8_t   status = 1;
};

struct Message {
    int64_t  id = 0;
    int64_t  conversation_id = 0;
    int64_t  sender_id = 0;
    int64_t  seq = 0;
    int8_t   msg_type = 0;
    std::string content;
    int8_t   status = 0;          // 0正常 1已撤回 2已删除
    std::string client_msg_id;
};

struct Conversation {
    int64_t  id = 0;
    int8_t   type = 0;            // 1私聊 2群聊
    std::string name;
    int64_t  owner_id = 0;
    int64_t  max_seq = 0;
};

struct ConversationMember {
    int64_t  conversation_id = 0;
    int64_t  user_id = 0;
    int8_t   role = 0;            // 0成员 1管理员 2群主
    int64_t  last_read_seq = 0;
    int64_t  last_ack_seq = 0;
    int8_t   mute = 0;
};

} // namespace nova
