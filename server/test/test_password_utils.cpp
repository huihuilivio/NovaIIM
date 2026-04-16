// test_password_utils.cpp — PasswordUtils 单元测试
// 覆盖: Hash 格式、Verify 正/错、随机盐、边界值

#include <gtest/gtest.h>
#include <string>

#include "admin/password_utils.h"

namespace nova {
namespace {

// ============================================================
// Hash 格式验证
// ============================================================

TEST(PasswordUtilsTest, HashProducesNonEmptyString) {
    auto h = PasswordUtils::Hash("password123");
    EXPECT_FALSE(h.empty());
}

TEST(PasswordUtilsTest, HashHasExpectedFormat) {
    // 格式: pbkdf2:sha256:<iterations>$<salt_hex>$<hash_hex>
    auto h = PasswordUtils::Hash("test");
    EXPECT_EQ(h.substr(0, 14), "pbkdf2:sha256:");

    // 两个 '$' 分隔符
    auto d1 = h.find('$');
    auto d2 = h.find('$', d1 + 1);
    EXPECT_NE(d1, std::string::npos);
    EXPECT_NE(d2, std::string::npos);

    // salt_hex 长度 = 16 bytes * 2 = 32 chars
    std::string salt_hex = h.substr(d1 + 1, d2 - d1 - 1);
    EXPECT_EQ(salt_hex.size(), 32u);

    // hash_hex 长度 = 32 bytes * 2 = 64 chars
    std::string hash_hex = h.substr(d2 + 1);
    EXPECT_EQ(hash_hex.size(), 64u);
}

TEST(PasswordUtilsTest, HashContainsIterations) {
    auto h = PasswordUtils::Hash("x");
    // iterations 字段在 "pbkdf2:sha256:" 之后，"$" 之前
    auto prefix_len      = std::string("pbkdf2:sha256:").size();
    auto d1              = h.find('$');
    std::string iter_str = h.substr(prefix_len, d1 - prefix_len);
    int iter             = std::stoi(iter_str);
    EXPECT_GE(iter, 10000);  // 至少 10k 次迭代，不低于最低安全标准
}

// ============================================================
// Verify 正流程
// ============================================================

TEST(PasswordUtilsTest, VerifyCorrectPassword) {
    const std::string pw = "correct-horse-battery-staple";
    auto hash            = PasswordUtils::Hash(pw);
    EXPECT_TRUE(PasswordUtils::Verify(pw, hash));
}

TEST(PasswordUtilsTest, VerifyWrongPassword) {
    auto hash = PasswordUtils::Hash("correct");
    EXPECT_FALSE(PasswordUtils::Verify("wrong", hash));
}

TEST(PasswordUtilsTest, VerifyCaseSensitive) {
    auto hash = PasswordUtils::Hash("Password123");
    EXPECT_FALSE(PasswordUtils::Verify("password123", hash));
    EXPECT_FALSE(PasswordUtils::Verify("PASSWORD123", hash));
    EXPECT_TRUE(PasswordUtils::Verify("Password123", hash));
}

TEST(PasswordUtilsTest, VerifySpecialCharacters) {
    const std::string pw = "p@$$w0rd!#&*()\n\t";
    auto hash            = PasswordUtils::Hash(pw);
    EXPECT_TRUE(PasswordUtils::Verify(pw, hash));
    EXPECT_FALSE(PasswordUtils::Verify("p@$$w0rd!#&*()", hash));
}

// ============================================================
// 随机盐
// ============================================================

TEST(PasswordUtilsTest, TwoHashesOfSamePasswordDiffer) {
    const std::string pw = "same-password";
    auto h1              = PasswordUtils::Hash(pw);
    auto h2              = PasswordUtils::Hash(pw);
    // 随机盐导致两个哈希不同
    EXPECT_NE(h1, h2);
    // 但两者都能验证通过
    EXPECT_TRUE(PasswordUtils::Verify(pw, h1));
    EXPECT_TRUE(PasswordUtils::Verify(pw, h2));
}

// ============================================================
// 边界值 & 错误格式
// ============================================================

TEST(PasswordUtilsTest, EmptyPasswordHashAndVerify) {
    auto hash = PasswordUtils::Hash("");
    // 空密码也应生成合法哈希
    EXPECT_FALSE(hash.empty());
    EXPECT_TRUE(PasswordUtils::Verify("", hash));
    EXPECT_FALSE(PasswordUtils::Verify("x", hash));
}

TEST(PasswordUtilsTest, LongPasswordHashAndVerify) {
    const std::string pw(1000, 'a');
    auto hash = PasswordUtils::Hash(pw);
    EXPECT_TRUE(PasswordUtils::Verify(pw, hash));
    EXPECT_FALSE(PasswordUtils::Verify(pw.substr(0, 999), hash));
}

TEST(PasswordUtilsTest, VerifyMalformedHashReturnsFalse) {
    EXPECT_FALSE(PasswordUtils::Verify("pw", "not-a-hash"));
    EXPECT_FALSE(PasswordUtils::Verify("pw", ""));
    EXPECT_FALSE(PasswordUtils::Verify("pw", "pbkdf2:sha256:100000$bad"));
    EXPECT_FALSE(PasswordUtils::Verify("pw", "md5:plain_text_hash!"));
}

}  // namespace
}  // namespace nova
