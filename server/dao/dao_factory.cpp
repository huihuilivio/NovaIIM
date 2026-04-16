#include "dao_factory.h"
#include "sqlite3/sqlite_dao_factory.h"
#ifdef ORMPP_ENABLE_MYSQL
#include "mysql/mysql_dao_factory.h"
#endif
#include "../core/app_config.h"

#include <stdexcept>

namespace nova {

std::unique_ptr<DaoFactory> CreateDaoFactory(const DatabaseConfig& config) {
    if (config.type == "sqlite") {
        return std::make_unique<SqliteDaoFactory>(config.path);
    }
#ifdef ORMPP_ENABLE_MYSQL
    if (config.type == "mysql") {
        return std::make_unique<MysqlDaoFactory>(config);
    }
#endif
    throw std::runtime_error("unsupported database type: " + config.type);
}

}  // namespace nova
