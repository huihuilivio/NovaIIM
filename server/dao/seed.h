#pragma once

#include "../core/password_utils.h"
#include "../core/defer.h"
#include "../model/types.h"

#include <spdlog/spdlog.h>

#include <string>

namespace nova {

/// 初始化种子数据：权限 + 超管角色 + 超管账户
/// 仅在表为空时插入（首次运行），幂等安全
/// 全部在一个事务内完成，失败自动回滚，保证全有全无
template <typename DbMgr>
void SeedSuperAdmin(DbMgr& db) {
    auto&& db_conn = db.DB();

    // 检查 admin_roles 表（整个 seed 流程的最后一步）
    // 若已有记录，说明 seed 已完整执行过
    auto ar_count = db_conn.template query_s<std::tuple<int64_t>>("SELECT count(*) FROM admin_roles");
    if (!ar_count.empty() && std::get<0>(ar_count[0]) > 0) {
        return;
    }

    SPDLOG_INFO("First run detected, seeding super admin...");

    // 开启事务：任何一步失败则整体回滚
    if (!db_conn.execute("BEGIN")) {
        SPDLOG_ERROR("Failed to seed: cannot begin transaction");
        return;
    }

    // RAII 回滚守卫：析构时自动 ROLLBACK（除非已 COMMIT）
    bool committed = false;
    NOVA_DEFER {
        if (!committed) {
            db_conn.execute("ROLLBACK");
            SPDLOG_ERROR("Seed transaction rolled back due to failure");
        }
    };

    // ---- 1. 插入权限 ----
    struct PermDef {
        const char* name;
        const char* code;
    };
    static constexpr PermDef kPerms[] = {
        {"管理员登录", "admin.login"},  {"仪表盘查看", "admin.dashboard"}, {"审计日志查看", "admin.audit"},
        {"管理员管理", "admin.manage"}, {"系统配置", "admin.config"},      {"用户查看", "user.view"},
        {"用户创建", "user.create"},    {"用户编辑", "user.edit"},         {"用户删除", "user.delete"},
        {"用户封禁", "user.ban"},       {"消息查看", "msg.view"},          {"消息撤回", "msg.recall"},
        {"消息管理", "msg.delete_all"},
    };

    for (auto& [name, code] : kPerms) {
        Permission p;
        p.name = name;
        p.code = code;
        if (db_conn.insert(p) != 1) {
            SPDLOG_ERROR("Failed to seed: insert permission '{}' failed", code);
            return;
        }
    }

    // ---- 2. 插入超管角色 ----
    Role role;
    role.name        = "超级管理员";
    role.code        = "super_admin";
    role.description = "拥有所有权限";
    auto role_id     = static_cast<int64_t>(db_conn.get_insert_id_after_insert(role));
    if (role_id <= 0) {
        SPDLOG_ERROR("Failed to seed: insert super_admin role failed");
        return;
    }

    // ---- 3. 绑定所有权限到超管角色 ----
    auto all_perms = db_conn.template query_s<std::tuple<int64_t>>("SELECT id FROM permissions");
    if (all_perms.empty()) {
        SPDLOG_ERROR("Failed to seed: no permissions found after insert");
        return;
    }
    for (auto& [perm_id] : all_perms) {
        RolePermission rp;
        rp.role_id       = role_id;
        rp.permission_id = perm_id;
        if (db_conn.insert(rp) != 1) {
            SPDLOG_ERROR("Failed to seed: bind permission {} to role failed", perm_id);
            return;
        }
    }

    // ---- 4. 创建超管账户 ----
    auto hash = PasswordUtils::Hash("nova2024");
    if (hash.empty()) {
        SPDLOG_ERROR("Failed to seed: password hashing failed");
        return;
    }

    Admin admin;
    admin.uid           = "admin";
    admin.password_hash = hash;
    admin.nickname      = "Administrator";
    auto admin_id       = static_cast<int64_t>(db_conn.get_insert_id_after_insert(admin));
    if (admin_id <= 0) {
        SPDLOG_ERROR("Failed to seed: insert admin account failed");
        return;
    }

    // ---- 5. 绑定超管角色到管理员账户 ----
    AdminRole ar;
    ar.admin_id = admin_id;
    ar.role_id  = role_id;
    if (db_conn.insert(ar) != 1) {
        SPDLOG_ERROR("Failed to seed: bind role to admin failed");
        return;
    }

    // 提交事务
    if (!db_conn.execute("COMMIT")) {
        SPDLOG_ERROR("Failed to seed: COMMIT failed");
        return;  // ~RollbackGuard will ROLLBACK
    }
    committed = true;

    SPDLOG_INFO("Super admin seeded: uid=admin (change password after first login!)");
}

}  // namespace nova
