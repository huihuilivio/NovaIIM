#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../model/types.h"

namespace nova {

struct AuditLogListResult {
    std::vector<AuditLog> items;
    int64_t total = 0;
};

class AuditLogDao {
public:
    virtual ~AuditLogDao() = default;

    virtual bool Insert(const AuditLog& log) = 0;

    // 分页查询审计日志
    // user_id=0 表示不过滤，action="" 表示不过滤
    virtual AuditLogListResult List(int64_t user_id, const std::string& action,
                                    const std::string& start_time,
                                    const std::string& end_time,
                                    int page, int page_size) = 0;
};

} // namespace nova
