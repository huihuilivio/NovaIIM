#pragma once

#include "../message_dao.h"

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class MessageDaoImplT : public MessageDao {
public:
    explicit MessageDaoImplT(DbMgr& db) : db_(db) {}

    bool Insert(const Message& msg) override;
    std::vector<Message> GetAfterSeq(int64_t conversation_id, int64_t after_seq, int limit) override;
    bool UpdateStatus(int64_t msg_id, int8_t status) override;
    MessageListResult ListMessages(int64_t conversation_id,
                                   const std::string& start_time,
                                   const std::string& end_time,
                                   int page, int page_size) override;
    std::optional<Message> FindById(int64_t id) override;

private:
    DbMgr& db_;
};

} // namespace nova
