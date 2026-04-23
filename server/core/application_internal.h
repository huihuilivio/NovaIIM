#pragma once
// Internal helpers — exposed for unit testing only.
// Do NOT depend on these from production code outside application.cpp.

#include "app_config.h"
#include <cstddef>

namespace nova {
class ServerContext;

namespace detail {

size_t RoundUpPow2(size_t n);
bool   InitDatabase(ServerContext& ctx, const DatabaseConfig& db_cfg);
bool   ValidateJwtSecret(const AdminConfig& admin_cfg);

}  // namespace detail
}  // namespace nova
