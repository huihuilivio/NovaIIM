#include "jwt_utils.h"

#include <l8w8jwt/encode.h>
#include <l8w8jwt/decode.h>
#include <l8w8jwt/claim.h>

#include <charconv>
#include <cstring>

namespace nova {

std::string JwtUtils::Sign(int64_t user_id,
                           std::string_view secret,
                           int expires_seconds,
                           JwtAlgorithm alg) {
    auto sub = std::to_string(user_id);

    struct l8w8jwt_encoding_params params;
    l8w8jwt_encoding_params_init(&params);

    params.alg = static_cast<int>(alg);

    params.iss        = const_cast<char*>(kIssuer);
    params.iss_length = std::strlen(kIssuer);

    params.sub        = sub.data();
    params.sub_length = sub.size();

    params.iat = l8w8jwt_time(nullptr);
    params.exp = l8w8jwt_time(nullptr) + expires_seconds;

    params.secret_key        = reinterpret_cast<unsigned char*>(const_cast<char*>(secret.data()));
    params.secret_key_length = secret.size();

    char*  jwt     = nullptr;
    size_t jwt_len = 0;
    params.out        = &jwt;
    params.out_length = &jwt_len;

    int rc = l8w8jwt_encode(&params);
    if (rc != L8W8JWT_SUCCESS || jwt == nullptr) {
        return {};
    }

    std::string result(jwt, jwt_len);
    l8w8jwt_free(jwt);
    return result;
}

std::optional<JwtClaims> JwtUtils::Verify(std::string_view token,
                                          std::string_view secret,
                                          JwtAlgorithm alg) {
    struct l8w8jwt_decoding_params params;
    l8w8jwt_decoding_params_init(&params);

    params.alg = static_cast<int>(alg);

    params.jwt        = const_cast<char*>(token.data());
    params.jwt_length = token.size();

    params.verification_key        = reinterpret_cast<unsigned char*>(const_cast<char*>(secret.data()));
    params.verification_key_length = secret.size();

    params.validate_iss        = const_cast<char*>(kIssuer);
    params.validate_iss_length = std::strlen(kIssuer);

    params.validate_exp            = 1;
    params.exp_tolerance_seconds   = 5;
    params.validate_iat            = 1;
    params.iat_tolerance_seconds   = 5;

    enum l8w8jwt_validation_result validation = L8W8JWT_VALID;
    struct l8w8jwt_claim* claims = nullptr;
    size_t claims_count = 0;

    int rc = l8w8jwt_decode(&params, &validation, &claims, &claims_count);
    if (rc != L8W8JWT_SUCCESS || validation != L8W8JWT_VALID) {
        if (claims) l8w8jwt_free_claims(claims, claims_count);
        return std::nullopt;
    }

    JwtClaims result;

    // 提取 sub → user_id
    auto* sub_claim = l8w8jwt_get_claim(claims, claims_count, "sub", 3);
    if (sub_claim && sub_claim->value) {
        std::from_chars(sub_claim->value,
                        sub_claim->value + sub_claim->value_length,
                        result.user_id);
    }

    // 提取 iat
    auto* iat_claim = l8w8jwt_get_claim(claims, claims_count, "iat", 3);
    if (iat_claim && iat_claim->value) {
        std::from_chars(iat_claim->value,
                        iat_claim->value + iat_claim->value_length,
                        result.iat);
    }

    // 提取 exp
    auto* exp_claim = l8w8jwt_get_claim(claims, claims_count, "exp", 3);
    if (exp_claim && exp_claim->value) {
        std::from_chars(exp_claim->value,
                        exp_claim->value + exp_claim->value_length,
                        result.exp);
    }

    l8w8jwt_free_claims(claims, claims_count);

    if (result.user_id == 0) {
        return std::nullopt;  // sub 缺失或无效
    }

    return result;
}

} // namespace nova
