#pragma once

#include "../audit_log_dao.h"

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class AuditLogDaoImplT : public AuditLogDao {
public:
    explicit AuditLogDaoImplT(DbMgr& db) : db_(db) {}

    bool Insert(const AuditLog& log) override;
    AuditLogListResult List(int64_t user_id, const std::string& action,
                            const std::string& start_time,
                            const std::string& end_time,
                            int page, int page_size) override;

private:
    DbMgr& db_;
};

} // namespace nova
