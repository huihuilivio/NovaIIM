#include "password_utils.h"

#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#include <cstring>
#include <sstream>
#include <iomanip>

namespace nova {

static constexpr int kIterations = 100000;
static constexpr int kSaltLen    = 16;
static constexpr int kHashLen    = 32;  // SHA-256 output

static std::string ToHex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

static unsigned char HexCharToByte(char c) {
    if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(c - 'A' + 10);
    return 255;  // Invalid hex character
}

static bool FromHex(const std::string& hex, unsigned char* out, size_t out_len) {
    if (hex.size() != out_len * 2)
        return false;
    for (size_t i = 0; i < out_len; ++i) {
        unsigned char high = HexCharToByte(hex[i * 2]);
        unsigned char low = HexCharToByte(hex[i * 2 + 1]);
        if (high == 255 || low == 255)
            return false;
        out[i] = (high << 4) | low;
    }
    return true;
}

std::string PasswordUtils::Hash(std::string_view password) {
    // 生成随机 salt
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    const char* pers = "nova_password";
    int ret          = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                             reinterpret_cast<const unsigned char*>(pers), std::strlen(pers));
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return {};
    }

    unsigned char salt[kSaltLen];
    ret = mbedtls_ctr_drbg_random(&ctr_drbg, salt, kSaltLen);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    if (ret != 0)
        return {};

    // PBKDF2-SHA256
    unsigned char hash[kHashLen];
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    ret                              = mbedtls_md_setup(&md_ctx, md_info, 1);
    if (ret != 0) {
        mbedtls_md_free(&md_ctx);
        return {};
    }

    ret = mbedtls_pkcs5_pbkdf2_hmac(&md_ctx, reinterpret_cast<const unsigned char*>(password.data()), password.size(),
                                    salt, kSaltLen, kIterations, kHashLen, hash);
    mbedtls_md_free(&md_ctx);
    if (ret != 0)
        return {};

    // 格式: pbkdf2:sha256:<iterations>$<salt_hex>$<hash_hex>
    std::ostringstream oss;
    oss << "pbkdf2:sha256:" << kIterations << "$" << ToHex(salt, kSaltLen) << "$" << ToHex(hash, kHashLen);
    return oss.str();
}

bool PasswordUtils::Verify(std::string_view password, std::string_view stored_hash) {
    // 解析格式: pbkdf2:sha256:<iterations>$<salt_hex>$<hash_hex>
    std::string s(stored_hash);

    // 前缀检查
    const std::string prefix = "pbkdf2:sha256:";
    if (s.substr(0, prefix.size()) != prefix)
        return false;
    s = s.substr(prefix.size());

    // 解析 iterations
    auto dollar1 = s.find('$');
    if (dollar1 == std::string::npos)
        return false;
    int iterations = std::atoi(s.substr(0, dollar1).c_str());
    if (iterations <= 0 || iterations > 10'000'000)
        return false;
    s = s.substr(dollar1 + 1);

    // 解析 salt 和 hash
    auto dollar2 = s.find('$');
    if (dollar2 == std::string::npos)
        return false;
    std::string salt_hex = s.substr(0, dollar2);
    std::string hash_hex = s.substr(dollar2 + 1);

    unsigned char salt[kSaltLen];
    unsigned char expected_hash[kHashLen];
    if (!FromHex(salt_hex, salt, kSaltLen))
        return false;
    if (!FromHex(hash_hex, expected_hash, kHashLen))
        return false;

    // 重新计算
    unsigned char computed_hash[kHashLen];
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&md_ctx, md_info, 1) != 0) {
        mbedtls_md_free(&md_ctx);
        return false;
    }

    int ret = mbedtls_pkcs5_pbkdf2_hmac(&md_ctx, reinterpret_cast<const unsigned char*>(password.data()),
                                        password.size(), salt, kSaltLen, iterations, kHashLen, computed_hash);
    mbedtls_md_free(&md_ctx);
    if (ret != 0)
        return false;

    // 常量时间比较，防止时序攻击
    int diff = 0;
    for (int i = 0; i < kHashLen; ++i) {
        diff |= computed_hash[i] ^ expected_hash[i];
    }
    return diff == 0;
}

}  // namespace nova
