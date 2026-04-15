#pragma once

#include "../admin/password_utils.h"
#include "../model/types.h"

#include <spdlog/spdlog.h>

#include <array>
#include <string>

namespace nova {

/// 初始化种子数据：权限 + 超管角色 + 超管账户
/// 仅在表为空时插入（首次运行），幂等安全
template <typename DbMgr>
void SeedSuperAdmin(DbMgr& db) {
    // 如果已有管理员，跳过（非首次运行）
    auto admin_count = db.DB().template query_s<std::tuple<int64_t>>(
        "SELECT count(*) FROM admins");
    if (!admin_count.empty() && std::get<0>(admin_count[0]) > 0) {
        return;
    }

    SPDLOG_INFO("First run detected, seeding super admin...");

    // ---- 1. 插入权限 ----
    struct PermDef { const char* name; const char* code; };
    static constexpr std::array<PermDef, 10> kPerms = {{
        {"管理员登录",   "admin.login"},
        {"仪表盘查看",   "admin.dashboard"},
        {"审计日志查看", "admin.audit"},
        {"用户查看",     "user.view"},
        {"用户创建",     "user.create"},
        {"用户编辑",     "user.edit"},
        {"用户删除",     "user.delete"},
        {"用户封禁",     "user.ban"},
        {"消息管理",     "msg.delete_all"},
        {"系统配置",     "admin.config"},
    }};

    for (auto& [name, code] : kPerms) {
        Permission p;
        p.name = name;
        p.code = code;
        db.DB().insert(p);
    }

    // ---- 2. 插入超管角色 ----
    Role role;
    role.name        = "超级管理员";
    role.code        = "super_admin";
    role.description = "拥有所有权限";
    db.DB().insert(role);

    // 查询刚插入的 role_id
    auto role_rows = db.DB().template query_s<std::tuple<int64_t>>(
        "SELECT id FROM roles WHERE code = ?", std::string("super_admin"));
    if (role_rows.empty()) {
        SPDLOG_ERROR("Failed to seed: cannot find super_admin role");
        return;
    }
    int64_t role_id = std::get<0>(role_rows[0]);

    // ---- 3. 绑定所有权限到超管角色 ----
    auto all_perms = db.DB().template query_s<std::tuple<int64_t>>(
        "SELECT id FROM permissions");
    for (auto& [perm_id] : all_perms) {
        RolePermission rp;
        rp.role_id       = role_id;
        rp.permission_id = perm_id;
        db.DB().insert(rp);
    }

    // ---- 4. 创建超管账户 ----
    auto hash = PasswordUtils::Hash("admin123");
    if (hash.empty()) {
        SPDLOG_ERROR("Failed to seed: password hashing failed");
        return;
    }

    Admin admin;
    admin.uid           = "admin";
    admin.password_hash = hash;
    admin.nickname      = "Administrator";
    db.DB().insert(admin);

    // 查询刚插入的 admin_id
    auto admin_rows = db.DB().template query_s<std::tuple<int64_t>>(
        "SELECT id FROM admins WHERE uid = ?", std::string("admin"));
    if (admin_rows.empty()) {
        SPDLOG_ERROR("Failed to seed: cannot find admin account");
        return;
    }
    int64_t admin_id = std::get<0>(admin_rows[0]);

    // ---- 5. 绑定超管角色到管理员账户 ----
    AdminRole ar;
    ar.admin_id = admin_id;
    ar.role_id  = role_id;
    db.DB().insert(ar);

    SPDLOG_INFO("Super admin seeded: uid=admin (change password after first login!)");
}

}  // namespace nova
