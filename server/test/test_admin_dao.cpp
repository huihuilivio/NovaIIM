// test_admin_dao.cpp — Admin DAO 层单元测试
// 使用内存 SQLite（":memory:"），每个测试 Fixture 独立数据库实例
// 覆盖: AdminAccountDao / AdminSessionDao / RbacDao

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "dao/dao_factory.h"
#include "dao/admin_account_dao.h"
#include "dao/admin_session_dao.h"
#include "dao/rbac_dao.h"
#include "admin/password_utils.h"
#include "core/app_config.h"

namespace nova {
namespace {

// 创建内存数据库（每个测试独享）
// 注意: 多次调用 CreateDaoFactory 时各自独立，互不影响
static std::unique_ptr<DaoFactory> MakeMemoryDb() {
    DatabaseConfig cfg;
    cfg.type = "sqlite";
    cfg.path = ":memory:";
    return CreateDaoFactory(cfg);
}

// ============================================================
// AdminAccountDao 测试
// ============================================================

class AdminAccountDaoTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory_ = MakeMemoryDb();
    }

    DaoFactory& dao() { return *factory_; }

    std::unique_ptr<DaoFactory> factory_;
};

TEST_F(AdminAccountDaoTest, SeedCreatesDefaultAdmin) {
    // SeedSuperAdmin 在工厂初始化时自动运行
    auto admin = dao().AdminAccount().FindByUid("admin");
    ASSERT_TRUE(admin.has_value());
    EXPECT_EQ(admin->uid, "admin");
    EXPECT_EQ(admin->status, 1);
}

TEST_F(AdminAccountDaoTest, FindByUidNotFound) {
    auto result = dao().AdminAccount().FindByUid("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(AdminAccountDaoTest, InsertAndFindByUid) {
    Admin a;
    a.uid           = "operator1";
    a.nickname      = "运营员";
    a.password_hash = PasswordUtils::Hash("pass123");
    a.status        = 1;

    EXPECT_TRUE(dao().AdminAccount().Insert(a));
    EXPECT_GT(a.id, 0);  // id 被填充

    auto found = dao().AdminAccount().FindByUid("operator1");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->nickname, "运营员");
    EXPECT_EQ(found->status, 1);
    EXPECT_EQ(found->id, a.id);
}

TEST_F(AdminAccountDaoTest, FindById) {
    // 默认 admin 的 id 应为 1（第一条插入记录）
    auto admin = dao().AdminAccount().FindByUid("admin");
    ASSERT_TRUE(admin.has_value());

    auto by_id = dao().AdminAccount().FindById(admin->id);
    ASSERT_TRUE(by_id.has_value());
    EXPECT_EQ(by_id->uid, "admin");
}

TEST_F(AdminAccountDaoTest, FindByIdNotFound) {
    auto result = dao().AdminAccount().FindById(99999);
    EXPECT_FALSE(result.has_value());
}

TEST_F(AdminAccountDaoTest, UpdatePassword) {
    auto admin = dao().AdminAccount().FindByUid("admin");
    ASSERT_TRUE(admin.has_value());

    const std::string new_pass = "brand-new-password";
    std::string new_hash = PasswordUtils::Hash(new_pass);
    EXPECT_TRUE(dao().AdminAccount().UpdatePassword(admin->id, new_hash));

    // 新密码能验证通过
    auto updated = dao().AdminAccount().FindById(admin->id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_TRUE(PasswordUtils::Verify(new_pass, updated->password_hash));
    EXPECT_FALSE(PasswordUtils::Verify("nova2024", updated->password_hash));
}

TEST_F(AdminAccountDaoTest, UpdatePasswordNonExistentReturnsFalse) {
    EXPECT_FALSE(dao().AdminAccount().UpdatePassword(99999, "hash"));
}

TEST_F(AdminAccountDaoTest, DuplicateUidInsertFails) {
    Admin a;
    a.uid           = "admin";  // 与种子数据冲突
    a.nickname      = "dup";
    a.password_hash = PasswordUtils::Hash("x");
    a.status        = 1;

    // UNIQUE 约束应导致插入失败
    EXPECT_FALSE(dao().AdminAccount().Insert(a));
}

TEST_F(AdminAccountDaoTest, DeletedAdminNotVisible) {
    Admin a;
    a.uid           = "todelete";
    a.nickname      = "To Delete";
    a.password_hash = PasswordUtils::Hash("pass");
    a.status        = 3;  // status=3 表示已删除

    dao().AdminAccount().Insert(a);

    // FindByUid 过滤 status=3
    auto found = dao().AdminAccount().FindByUid("todelete");
    EXPECT_FALSE(found.has_value());
}

// ============================================================
// AdminSessionDao 测试
// ============================================================

class AdminSessionDaoTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory_ = MakeMemoryDb();
    }

    DaoFactory& dao() { return *factory_; }

    std::unique_ptr<DaoFactory> factory_;
};

TEST_F(AdminSessionDaoTest, NewTokenIsNotRevoked) {
    EXPECT_FALSE(dao().AdminSession().IsRevoked("some-token-hash"));
}

TEST_F(AdminSessionDaoTest, InsertAndIsNotRevokedByDefault) {
    AdminSession s;
    s.admin_id   = 1;
    s.token_hash = "hash-abc123";
    s.expires_at = "2099-12-31 00:00:00";
    s.revoked    = 0;

    EXPECT_TRUE(dao().AdminSession().Insert(s));
    EXPECT_FALSE(dao().AdminSession().IsRevoked("hash-abc123"));
}

TEST_F(AdminSessionDaoTest, RevokeByTokenHash) {
    AdminSession s;
    s.admin_id   = 1;
    s.token_hash = "hash-to-revoke";
    s.expires_at = "2099-12-31 00:00:00";
    s.revoked    = 0;
    dao().AdminSession().Insert(s);

    EXPECT_TRUE(dao().AdminSession().RevokeByTokenHash("hash-to-revoke"));
    EXPECT_TRUE(dao().AdminSession().IsRevoked("hash-to-revoke"));
}

TEST_F(AdminSessionDaoTest, RevokeByTokenHashNonExistentReturnsFalse) {
    EXPECT_FALSE(dao().AdminSession().RevokeByTokenHash("nonexistent-hash"));
}

TEST_F(AdminSessionDaoTest, RevokeByAdminRevokesAllSessions) {
    constexpr int64_t kAdminId = 7;

    for (int i = 0; i < 3; ++i) {
        AdminSession s;
        s.admin_id   = kAdminId;
        s.token_hash = "hash-" + std::to_string(i);
        s.expires_at = "2099-12-31 00:00:00";
        s.revoked    = 0;
        dao().AdminSession().Insert(s);
    }

    // 插入一个不同 admin 的 session，不应受影响
    AdminSession other;
    other.admin_id   = 999;
    other.token_hash = "other-hash";
    other.expires_at = "2099-12-31 00:00:00";
    other.revoked    = 0;
    dao().AdminSession().Insert(other);

    EXPECT_TRUE(dao().AdminSession().RevokeByAdmin(kAdminId));

    EXPECT_TRUE(dao().AdminSession().IsRevoked("hash-0"));
    EXPECT_TRUE(dao().AdminSession().IsRevoked("hash-1"));
    EXPECT_TRUE(dao().AdminSession().IsRevoked("hash-2"));
    EXPECT_FALSE(dao().AdminSession().IsRevoked("other-hash"));  // 不受影响
}

TEST_F(AdminSessionDaoTest, RevokeAlreadyRevokedIsIdempotent) {
    AdminSession s;
    s.admin_id   = 1;
    s.token_hash = "double-revoke";
    s.expires_at = "2099-12-31 00:00:00";
    s.revoked    = 0;
    dao().AdminSession().Insert(s);

    dao().AdminSession().RevokeByTokenHash("double-revoke");
    EXPECT_NO_THROW(dao().AdminSession().RevokeByTokenHash("double-revoke"));
    EXPECT_TRUE(dao().AdminSession().IsRevoked("double-revoke"));
}

// ============================================================
// RbacDao 测试
// ============================================================

class RbacDaoTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory_ = MakeMemoryDb();
    }

    DaoFactory& dao() { return *factory_; }

    // 获取种子数据中 admin 的 ID
    int64_t GetAdminId() {
        auto a = dao().AdminAccount().FindByUid("admin");
        return a ? a->id : 0;
    }

    std::unique_ptr<DaoFactory> factory_;
};

TEST_F(RbacDaoTest, SuperAdminHasPermissions) {
    int64_t id = GetAdminId();
    ASSERT_GT(id, 0);

    auto perms = dao().Rbac().GetUserPermissions(id);
    EXPECT_FALSE(perms.empty());
}

TEST_F(RbacDaoTest, SuperAdminHasLoginPermission) {
    int64_t id = GetAdminId();
    ASSERT_GT(id, 0);

    EXPECT_TRUE(dao().Rbac().HasPermission(id, "admin.login"));
}

TEST_F(RbacDaoTest, SuperAdminHasDashboardPermission) {
    int64_t id = GetAdminId();
    ASSERT_GT(id, 0);

    EXPECT_TRUE(dao().Rbac().HasPermission(id, "admin.dashboard"));
}

TEST_F(RbacDaoTest, SuperAdminHasAuditPermission) {
    int64_t id = GetAdminId();
    ASSERT_GT(id, 0);

    EXPECT_TRUE(dao().Rbac().HasPermission(id, "admin.audit"));
}

TEST_F(RbacDaoTest, SuperAdminHasUserPermissions) {
    int64_t id = GetAdminId();
    ASSERT_GT(id, 0);

    EXPECT_TRUE(dao().Rbac().HasPermission(id, "user.view"));
    EXPECT_TRUE(dao().Rbac().HasPermission(id, "user.create"));
    EXPECT_TRUE(dao().Rbac().HasPermission(id, "user.edit"));
    EXPECT_TRUE(dao().Rbac().HasPermission(id, "user.ban"));
}

TEST_F(RbacDaoTest, SuperAdminHasMessagePermission) {
    int64_t id = GetAdminId();
    ASSERT_GT(id, 0);

    EXPECT_TRUE(dao().Rbac().HasPermission(id, "msg.delete_all"));
}

TEST_F(RbacDaoTest, NonExistentAdminHasNoPermissions) {
    auto perms = dao().Rbac().GetUserPermissions(99999);
    EXPECT_TRUE(perms.empty());
    EXPECT_FALSE(dao().Rbac().HasPermission(99999, "admin.login"));
}

TEST_F(RbacDaoTest, AdminWithNoRoleHasNoPermissions) {
    // 插入一个没有绑定角色的管理员
    Admin a;
    a.uid           = "norole";
    a.nickname      = "NoRole";
    a.password_hash = PasswordUtils::Hash("x");
    a.status        = 1;
    dao().AdminAccount().Insert(a);

    auto perms = dao().Rbac().GetUserPermissions(a.id);
    EXPECT_TRUE(perms.empty());
    EXPECT_FALSE(dao().Rbac().HasPermission(a.id, "admin.login"));
}

TEST_F(RbacDaoTest, HasPermissionForNonExistentCodeReturnsFalse) {
    int64_t id = GetAdminId();
    ASSERT_GT(id, 0);

    EXPECT_FALSE(dao().Rbac().HasPermission(id, "nonexistent.permission"));
}

} // namespace
} // namespace nova
