#pragma once

#include <string>
#include <string_view>

namespace nova {

/// 密码哈希工具 —— 基于 PBKDF2-SHA256 (MbedTLS, l8w8jwt 已引入)
/// 格式: "pbkdf2:sha256:<iterations>$<salt_hex>$<hash_hex>"
class PasswordUtils {
public:
    /// 对明文密码生成哈希
    [[nodiscard]] static std::string Hash(std::string_view password);

    /// 验证明文密码是否匹配已存储的哈希
    [[nodiscard]] static bool Verify(std::string_view password, std::string_view stored_hash);

    PasswordUtils() = delete;
};

} // namespace nova
