#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "../model/types.h"

namespace nova {

// 文件 DAO 接口
class FileDao {
public:
    virtual ~FileDao() = default;

    // 插入文件记录，成功后 file.id 被填充
    virtual bool Insert(UserFile& file) = 0;

    // 按 ID 查询
    virtual std::optional<UserFile> FindById(int64_t id) = 0;

    // 查询用户某类型的最新文件（如最新头像）
    virtual std::optional<UserFile> FindLatestByUserAndType(int64_t user_id, const std::string& file_type) = 0;

    // 查询用户某类型的所有文件
    virtual std::vector<UserFile> ListByUserAndType(int64_t user_id, const std::string& file_type) = 0;

    // 软删除
    virtual bool SoftDelete(int64_t id) = 0;

    // 更新文件路径（Insert 后回填唯一路径）
    virtual bool UpdatePath(int64_t id, const std::string& path) = 0;

    // 将用户某类型的旧文件全部标记为已删除（换头像时清理旧记录）
    virtual bool SoftDeleteByUserAndType(int64_t user_id, const std::string& file_type) = 0;
};

}  // namespace nova
