#pragma once

#include "audit_log_dao.h"
#include "db_manager.h"

namespace nova {

class AuditLogDaoImpl : public AuditLogDao {
public:
    explicit AuditLogDaoImpl(DbManager& db) : db_(db) {}

    bool Insert(const AuditLog& log) override;
    AuditLogListResult List(int64_t user_id, const std::string& action,
                            const std::string& start_time,
                            const std::string& end_time,
                            int page, int page_size) override;

private:
    DbManager& db_;
};

} // namespace nova
