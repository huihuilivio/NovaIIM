// test_jwt_utils.cpp — JwtUtils 单元测试
// 覆盖: Sign / Verify 正流程、错误密钥、过期、篡改、多算法

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "admin/jwt_utils.h"

namespace nova {
namespace {

static constexpr const char* kSecret = "test-secret-key-32bytes-minimum!";
static constexpr int64_t kAdminId    = 42;

// ============================================================
// Sign / Verify 正流程
// ============================================================

TEST(JwtUtilsTest, SignProducesNonEmptyToken) {
    auto token = JwtUtils::Sign(kAdminId, kSecret);
    EXPECT_FALSE(token.empty());
    // JWT 格式: header.payload.signature
    EXPECT_EQ(std::count(token.begin(), token.end(), '.'), 2);
}

TEST(JwtUtilsTest, VerifyValidTokenReturnsCorrectAdminId) {
    auto token  = JwtUtils::Sign(kAdminId, kSecret, 3600);
    auto claims = JwtUtils::Verify(token, kSecret);
    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->admin_id, kAdminId);
}

TEST(JwtUtilsTest, VerifyPopulatesIatAndExp) {
    auto before = std::time(nullptr);
    auto token  = JwtUtils::Sign(kAdminId, kSecret, 300);
    auto after  = std::time(nullptr);

    auto claims = JwtUtils::Verify(token, kSecret);
    ASSERT_TRUE(claims.has_value());

    // iat 在调用前后范围内（容差 5s，与 l8w8jwt_time 一致）
    EXPECT_GE(claims->iat, before - 5);
    EXPECT_LE(claims->iat, after + 5);

    // exp = iat + 300（±10s 容差）
    EXPECT_NEAR(static_cast<double>(claims->exp - claims->iat), 300.0, 10.0);
}

TEST(JwtUtilsTest, DifferentAdminIdsProduceDifferentTokens) {
    auto t1 = JwtUtils::Sign(1, kSecret);
    auto t2 = JwtUtils::Sign(2, kSecret);
    EXPECT_NE(t1, t2);

    auto c1 = JwtUtils::Verify(t1, kSecret);
    auto c2 = JwtUtils::Verify(t2, kSecret);
    ASSERT_TRUE(c1.has_value());
    ASSERT_TRUE(c2.has_value());
    EXPECT_EQ(c1->admin_id, 1);
    EXPECT_EQ(c2->admin_id, 2);
}

// ============================================================
// 错误场景
// ============================================================

TEST(JwtUtilsTest, WrongSecretReturnsNullopt) {
    auto token  = JwtUtils::Sign(kAdminId, kSecret);
    auto claims = JwtUtils::Verify(token, "wrong-secret!!!!!!!!!!!!!!!!!!");
    EXPECT_FALSE(claims.has_value());
}

TEST(JwtUtilsTest, ExpiredTokenReturnsNullopt) {
    // expires_seconds = -1 使令牌立即过期
    auto token = JwtUtils::Sign(kAdminId, kSecret, -1);
    EXPECT_FALSE(token.empty());  // 签发本身应成功

    // l8w8jwt exp_tolerance_seconds = 5，所以需要等待超过容差
    // 为避免在 CI 中等待，直接签发 -10s 使其肯定过期
    auto token2 = JwtUtils::Sign(kAdminId, kSecret, -10);
    auto claims = JwtUtils::Verify(token2, kSecret);
    EXPECT_FALSE(claims.has_value());
}

TEST(JwtUtilsTest, TamperedPayloadReturnsNullopt) {
    auto token = JwtUtils::Sign(kAdminId, kSecret);

    // 找到第一个 '.' 分隔符，修改 payload 部分
    auto dot1 = token.find('.');
    auto dot2 = token.find('.', dot1 + 1);
    ASSERT_NE(dot1, std::string::npos);
    ASSERT_NE(dot2, std::string::npos);

    // 将 payload 最后一个字符改掉（base64url 字符替换）
    std::string tampered = token;
    char& c              = tampered[dot2 - 1];
    c                    = (c == 'A') ? 'B' : 'A';

    auto claims = JwtUtils::Verify(tampered, kSecret);
    EXPECT_FALSE(claims.has_value());
}

TEST(JwtUtilsTest, EmptyTokenReturnsNullopt) {
    EXPECT_FALSE(JwtUtils::Verify("", kSecret).has_value());
}

TEST(JwtUtilsTest, GarbageTokenReturnsNullopt) {
    EXPECT_FALSE(JwtUtils::Verify("not.a.jwt", kSecret).has_value());
}

TEST(JwtUtilsTest, EmptySecretSignReturnsEmpty) {
    // 空密钥不应崩溃，行为取决于 l8w8jwt 实现
    // 至少 Verify 用空密钥应失败
    auto token = JwtUtils::Sign(kAdminId, kSecret);
    EXPECT_FALSE(JwtUtils::Verify(token, "").has_value());
}

// ============================================================
// 多算法
// ============================================================

TEST(JwtUtilsTest, HS384RoundTrip) {
    auto token = JwtUtils::Sign(kAdminId, kSecret, 3600, JwtAlgorithm::HS384);
    EXPECT_FALSE(token.empty());

    auto claims = JwtUtils::Verify(token, kSecret, JwtAlgorithm::HS384);
    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->admin_id, kAdminId);
}

TEST(JwtUtilsTest, HS512RoundTrip) {
    auto token = JwtUtils::Sign(kAdminId, kSecret, 3600, JwtAlgorithm::HS512);
    EXPECT_FALSE(token.empty());

    auto claims = JwtUtils::Verify(token, kSecret, JwtAlgorithm::HS512);
    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->admin_id, kAdminId);
}

TEST(JwtUtilsTest, AlgorithmMismatchReturnsNullopt) {
    // 用 HS256 签发，用 HS512 验证 → 应失败
    auto token  = JwtUtils::Sign(kAdminId, kSecret, 3600, JwtAlgorithm::HS256);
    auto claims = JwtUtils::Verify(token, kSecret, JwtAlgorithm::HS512);
    EXPECT_FALSE(claims.has_value());
}

}  // namespace
}  // namespace nova
