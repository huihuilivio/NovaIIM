#include <gtest/gtest.h>
#include "core/application.h"
#include "core/application_internal.h"
#include "core/server_context.h"

#include <chrono>
#include <csignal>
#include <thread>

namespace nova {
namespace {

// ================================================================
// RoundUpPow2
// ================================================================

TEST(AppHelperTest, RoundUpPow2Zero)           { EXPECT_EQ(detail::RoundUpPow2(0), 2u); }
TEST(AppHelperTest, RoundUpPow2One)            { EXPECT_EQ(detail::RoundUpPow2(1), 2u); }
TEST(AppHelperTest, RoundUpPow2Two)            { EXPECT_EQ(detail::RoundUpPow2(2), 2u); }
TEST(AppHelperTest, RoundUpPow2Three)          { EXPECT_EQ(detail::RoundUpPow2(3), 4u); }
TEST(AppHelperTest, RoundUpPow2PowerOfTwo)     { EXPECT_EQ(detail::RoundUpPow2(8), 8u); }
TEST(AppHelperTest, RoundUpPow2NonPower)       { EXPECT_EQ(detail::RoundUpPow2(100), 128u); }
TEST(AppHelperTest, RoundUpPow2LargeAligned)   { EXPECT_EQ(detail::RoundUpPow2(1024), 1024u); }
TEST(AppHelperTest, RoundUpPow2LargeUnaligned) { EXPECT_EQ(detail::RoundUpPow2(1025), 2048u); }

// ================================================================
// InitDatabase
// ================================================================

TEST(AppHelperTest, InitDatabaseSuccess) {
    AppConfig cfg;
    cfg.db.type = "sqlite";
    cfg.db.path = ":memory:";
    ServerContext ctx(cfg);
    EXPECT_TRUE(detail::InitDatabase(ctx, cfg.db));
}

TEST(AppHelperTest, InitDatabaseUnsupportedType) {
    AppConfig cfg;
    cfg.db.type = "nosql";
    cfg.db.path = ":memory:";
    ServerContext ctx(cfg);
    EXPECT_FALSE(detail::InitDatabase(ctx, cfg.db));
}

// ================================================================
// ValidateJwtSecret (fail-fast on weak/default secrets when admin enabled)
// ================================================================

TEST(AppHelperTest, ValidateJwtSecretDisabledAllowsWeak) {
    AdminConfig a;
    a.enabled = false;
    a.jwt_secret = "change-me-in-production";
    EXPECT_TRUE(detail::ValidateJwtSecret(a));
}

TEST(AppHelperTest, ValidateJwtSecretEmptyRejected) {
    AdminConfig a;
    a.enabled = true;
    a.jwt_secret.clear();
    EXPECT_FALSE(detail::ValidateJwtSecret(a));
}

TEST(AppHelperTest, ValidateJwtSecretDefaultRejected) {
    AdminConfig a;
    a.enabled = true;
    a.jwt_secret = "change-me-in-production";
    EXPECT_FALSE(detail::ValidateJwtSecret(a));
}

TEST(AppHelperTest, ValidateJwtSecretShortRejected) {
    AdminConfig a;
    a.enabled = true;
    a.jwt_secret = "abc";
    EXPECT_FALSE(detail::ValidateJwtSecret(a));
}

TEST(AppHelperTest, ValidateJwtSecretStrongAccepted) {
    AdminConfig a;
    a.enabled = true;
    a.jwt_secret = "a-very-strong-secret-key-32chars!";  // 33 chars
    EXPECT_TRUE(detail::ValidateJwtSecret(a));
}

// ================================================================
// Application::Run — integration
// ================================================================

TEST(ApplicationRunTest, BadDatabaseReturnsOne) {
    AppConfig cfg;
    cfg.db.type = "invalid_db";
    cfg.admin.enabled = false;
    EXPECT_EQ(Application::Run(cfg), 1);
}

TEST(ApplicationRunTest, ValidConfigStartsAndStops) {
    AppConfig cfg;
    cfg.db.type = "sqlite";
    cfg.db.path = ":memory:";
    cfg.server.port = 19898;
    cfg.server.worker_threads = 1;
    cfg.server.queue_capacity = 5;  // non-power-of-2, exercises RoundUpPow2
    cfg.admin.enabled = false;

    std::thread t([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        std::raise(SIGINT);
    });

    int rc = Application::Run(cfg);
    t.join();
    EXPECT_EQ(rc, 0);
}

}  // namespace
}  // namespace nova
