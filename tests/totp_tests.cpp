#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "core/crypto/Crypto.h"
#include "core/crypto/Totp.h"

using namespace pm;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_totp;

// The RFC 6238 test secret is the ASCII bytes "12345678901234567890";
// this is its base32 encoding.
constexpr const char *kRfcSecretB32 = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";

} // namespace

TEST_CASE("base32Decode handles the RFC 4648 alphabet", "[crypto][totp]")
{
    const std::vector<std::uint8_t> expected = {'1', '2', '3', '4', '5', '6', '7',
                                                '8', '9', '0', '1', '2', '3', '4',
                                                '5', '6', '7', '8', '9', '0'};

    auto decoded = crypto::base32Decode(kRfcSecretB32);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == expected);

    // Lowercase, separators, and trailing padding are all tolerated (sites
    // format secrets as 'gezd gnbv ...' and some emit '=' padding).
    REQUIRE(crypto::base32Decode("gezd gnbv gy3t qojq gezd gnbv gy3t qojq")
            == expected);
    REQUIRE(crypto::base32Decode("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ====")
            == expected);

    // Invalid inputs → nullopt: illegal chars ('1'/'8' are not base32),
    // padding in the middle, empty string.
    REQUIRE_FALSE(crypto::base32Decode("GEZD1NBV").has_value());
    REQUIRE_FALSE(crypto::base32Decode("GE=ZD").has_value());
    REQUIRE_FALSE(crypto::base32Decode("").has_value());
    REQUIRE_FALSE(crypto::base32Decode("   ").has_value());
}

TEST_CASE("totpCode matches the RFC 6238 Appendix B SHA1 vectors",
          "[crypto][totp]")
{
    // (time, expected 8-digit TOTP) — the official interoperability vectors.
    const struct
    {
        std::int64_t time;
        const char *expected;
    } vectors[] = {
        {59, "94287082"},          {1111111109, "07081804"},
        {1111111111, "14050471"},  {1234567890, "89005924"},
        {2000000000, "69279037"},  {20000000000, "65353130"},
    };

    for (const auto &v : vectors) {
        INFO("time = " << v.time);
        const auto code = crypto::totpCode(kRfcSecretB32, v.time, /*digits=*/8);
        REQUIRE(code.has_value());
        REQUIRE(*code == v.expected);
    }
}

TEST_CASE("totpCode produces authenticator-style 6-digit codes",
          "[crypto][totp]")
{
    // 6 digits = the last 6 of the 8-digit vector (mod 10^6 of the same value).
    const auto code = crypto::totpCode(kRfcSecretB32, 59);
    REQUIRE(code.has_value());
    REQUIRE(*code == "287082");
    REQUIRE(code->size() == 6);

    // Stable within one 30s window, different across windows.
    REQUIRE(crypto::totpCode(kRfcSecretB32, 30) == crypto::totpCode(kRfcSecretB32, 59));
    REQUIRE(crypto::totpCode(kRfcSecretB32, 59) != crypto::totpCode(kRfcSecretB32, 60));

    // Zero-padding is preserved (a code may legitimately start with 0).
    const auto padded = crypto::totpCode(kRfcSecretB32, 1111111109, 8);
    REQUIRE(padded.has_value());
    REQUIRE((*padded)[0] == '0');
}

TEST_CASE("totpCode rejects invalid inputs", "[crypto][totp]")
{
    REQUIRE_FALSE(crypto::totpCode("not!base32", 59).has_value());
    REQUIRE_FALSE(crypto::totpCode("", 59).has_value());
    REQUIRE_FALSE(crypto::totpCode(kRfcSecretB32, 59, /*digits=*/5).has_value());
    REQUIRE_FALSE(crypto::totpCode(kRfcSecretB32, 59, /*digits=*/9).has_value());
    REQUIRE_FALSE(crypto::totpCode(kRfcSecretB32, 59, 6, /*period=*/0).has_value());
    REQUIRE_FALSE(crypto::totpCode(kRfcSecretB32, -1).has_value());
}

TEST_CASE("totpSecondsRemaining counts down to the rollover", "[crypto][totp]")
{
    REQUIRE(crypto::totpSecondsRemaining(0) == 30);
    REQUIRE(crypto::totpSecondsRemaining(1) == 29);
    REQUIRE(crypto::totpSecondsRemaining(29) == 1);
    REQUIRE(crypto::totpSecondsRemaining(30) == 30);
    REQUIRE(crypto::totpSecondsRemaining(59) == 1);
}
