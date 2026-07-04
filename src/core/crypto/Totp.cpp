#include "core/crypto/Totp.h"

#include <array>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sodium.h>

namespace pm::crypto {
namespace {

// Value of one base32 alphabet character (A-Z2-7, case-insensitive), or -1.
int base32Value(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a';
    if (c >= '2' && c <= '7')
        return c - '2' + 26;
    return -1;
}

} // namespace

std::optional<std::vector<std::uint8_t>> base32Decode(std::string_view text)
{
    std::vector<std::uint8_t> out;
    out.reserve(text.size() * 5 / 8);

    std::uint32_t buffer = 0;
    int bits = 0;
    bool paddingSeen = false;

    for (char c : text) {
        if (c == ' ' || c == '-') {
            continue; // display separators
        }
        if (c == '=') {
            paddingSeen = true; // only trailing padding is valid
            continue;
        }
        const int value = base32Value(c);
        if (value < 0 || paddingSeen) {
            return std::nullopt;
        }
        buffer = (buffer << 5) | static_cast<std::uint32_t>(value);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xff));
        }
    }

    if (out.empty()) {
        return std::nullopt;
    }
    return out;
}

std::optional<std::string> totpCode(std::string_view base32Secret,
                                    std::int64_t unixTime, int digits,
                                    int periodSeconds)
{
    if (digits < 6 || digits > 8 || periodSeconds <= 0 || unixTime < 0) {
        return std::nullopt;
    }

    auto secret = base32Decode(base32Secret);
    if (!secret) {
        return std::nullopt;
    }

    // RFC 6238: counter = floor(time / period), as a big-endian 64-bit message.
    const auto counter =
        static_cast<std::uint64_t>(unixTime / periodSeconds);
    std::array<std::uint8_t, 8> message{};
    for (int i = 0; i < 8; ++i) {
        message[static_cast<std::size_t>(7 - i)] =
            static_cast<std::uint8_t>((counter >> (8 * i)) & 0xff);
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> mac{};
    unsigned int macLen = 0;
    const bool ok = HMAC(EVP_sha1(), secret->data(),
                         static_cast<int>(secret->size()), message.data(),
                         message.size(), mac.data(), &macLen)
                    != nullptr;
    sodium_memzero(secret->data(), secret->size()); // secret's job is done
    if (!ok || macLen < 20) {
        return std::nullopt;
    }

    // RFC 4226 dynamic truncation: a 31-bit big-endian slice at a
    // hash-dependent offset, reduced to the requested number of digits.
    const unsigned offset = mac[19] & 0x0fU;
    const std::uint32_t binCode =
        (static_cast<std::uint32_t>(mac[offset] & 0x7f) << 24)
        | (static_cast<std::uint32_t>(mac[offset + 1]) << 16)
        | (static_cast<std::uint32_t>(mac[offset + 2]) << 8)
        | static_cast<std::uint32_t>(mac[offset + 3]);

    std::uint32_t modulus = 1;
    for (int i = 0; i < digits; ++i) {
        modulus *= 10;
    }

    std::string code = std::to_string(binCode % modulus);
    code.insert(0, static_cast<std::size_t>(digits) - code.size(), '0');
    return code;
}

int totpSecondsRemaining(std::int64_t unixTime, int periodSeconds) noexcept
{
    if (periodSeconds <= 0 || unixTime < 0) {
        return 0;
    }
    return periodSeconds - static_cast<int>(unixTime % periodSeconds);
}

} // namespace pm::crypto
