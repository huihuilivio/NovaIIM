#pragma once

#include "../admin_session_dao.h"

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class AdminSessionDaoImplT : public AdminSessionDao {
public:
    explicit AdminSessionDaoImplT(DbMgr& db) : db_(db) {}

    bool Insert(const AdminSession& session) override;
    bool IsRevoked(const std::string& token_hash) override;
    bool RevokeByUser(int64_t user_id) override;
    bool RevokeByTokenHash(const std::string& token_hash) override;

private:
    DbMgr& db_;
};

} // namespace nova
