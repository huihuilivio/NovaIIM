#pragma once

#include "../file_dao.h"

namespace nova {

class SqliteDbManager;  // forward

template <typename DbMgr>
class FileDaoImplT : public FileDao {
public:
    explicit FileDaoImplT(DbMgr& db) : db_(db) {}

    bool Insert(UserFile& file) override;
    std::optional<UserFile> FindById(int64_t id) override;
    std::optional<UserFile> FindLatestByUserAndType(int64_t user_id, const std::string& file_type) override;
    std::vector<UserFile> ListByUserAndType(int64_t user_id, const std::string& file_type) override;
    bool SoftDelete(int64_t id) override;
    bool SoftDeleteByUserAndType(int64_t user_id, const std::string& file_type) override;

private:
    DbMgr& db_;
};

}  // namespace nova
