#include "sync_service.h"

namespace nova {

void SyncService::HandleSyncMsg(ConnectionPtr conn, Packet& pkt) {
    // TODO: 1. 解码 body → { conversation_id, last_seq }
    // TODO: 2. SELECT * FROM messages WHERE conversation_id=? AND seq > last_seq ORDER BY seq LIMIT N
    // TODO: 3. 打包返回 SyncMsgResp
}

void SyncService::HandleSyncUnread(ConnectionPtr conn, Packet& pkt) {
    // TODO: 1. 查 conversation_members WHERE user_id = ?
    // TODO: 2. 计算各会话未读数 = max_seq - last_read_seq
    // TODO: 3. 打包返回 SyncUnreadResp
}

} // namespace nova
