#pragma once

#include "admin_session_dao.h"
#include "db_manager.h"

namespace nova {

class AdminSessionDaoImpl : public AdminSessionDao {
public:
    explicit AdminSessionDaoImpl(DbManager& db) : db_(db) {}

    bool Insert(const AdminSession& session) override;
    bool IsRevoked(const std::string& token_hash) override;
    bool RevokeByUser(int64_t user_id) override;
    bool RevokeByTokenHash(const std::string& token_hash) override;

private:
    DbManager& db_;
};

} // namespace nova
