#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

namespace nova {

struct JwtClaims {
    int64_t admin_id = 0;
    int64_t iat      = 0;
    int64_t exp      = 0;
};

/// HMAC 签名算法
enum class JwtAlgorithm : int {
    HS256 = 0,  // L8W8JWT_ALG_HS256
    HS384 = 1,  // L8W8JWT_ALG_HS384
    HS512 = 2,  // L8W8JWT_ALG_HS512
};

/// 轻量 JWT 工具 —— 封装 l8w8jwt (HMAC-SHA2)
/// 所有方法均为无状态静态调用，零额外开销
class JwtUtils {
public:
    static constexpr const char* kIssuer = "nova-admin";

    /// 签发 JWT
    /// @param admin_id  管理员 ID，写入 sub claim
    /// @param secret    HMAC 密钥
    /// @param expires_seconds 有效期（秒），默认 86400（24h）
    /// @param alg       签名算法，默认 HS256
    /// @return token 字符串；签发失败返回空字符串
    [[nodiscard]]
    static std::string Sign(int64_t admin_id,
                            std::string_view secret,
                            int expires_seconds = 86400,
                            JwtAlgorithm alg = JwtAlgorithm::HS256);

    /// 验证并解码 JWT
    /// @param token  待验证的 JWT 字符串
    /// @param secret HMAC 密钥（需与签发时一致）
    /// @param alg    签名算法（需与签发时一致）
    /// @return 验证通过返回 JwtClaims；过期/签名错误/格式非法返回 nullopt
    [[nodiscard]]
    static std::optional<JwtClaims> Verify(std::string_view token,
                                           std::string_view secret,
                                           JwtAlgorithm alg = JwtAlgorithm::HS256);

    JwtUtils() = delete;
};

} // namespace nova
