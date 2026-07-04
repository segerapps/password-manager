#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// RFC 6238 TOTP (time-based one-time passwords) over HMAC-SHA1 — the profile
// used by GitHub/Google/Microsoft authenticators. The stored secret is the
// base32 string the site shows at 2FA setup (the QR code's contents); the
// 6-digit code is derived from it and the current time, never stored.
//
// HMAC-SHA1 comes from OpenSSL (already shipped as SQLCipher's crypto backend);
// SHA1-in-HMAC is not affected by SHA1 collision attacks, and a TOTP mismatch
// only ever means a rejected login, not a confidentiality loss.
namespace pm::crypto {

// RFC 4648 base32 decode. Case-insensitive; spaces and '-' separators are
// ignored (sites format secrets as 'jbsw y3dp ...'); trailing '=' padding is
// optional. Returns nullopt on any other character or on an empty input.
std::optional<std::vector<std::uint8_t>> base32Decode(std::string_view text);

// The TOTP code for `unixTime`, as a zero-padded string of `digits` (6–8, per
// RFC 6238). Returns nullopt on an invalid secret, digits, or period.
std::optional<std::string> totpCode(std::string_view base32Secret,
                                    std::int64_t unixTime, int digits = 6,
                                    int periodSeconds = 30);

// Seconds until the code for `unixTime` rolls over (1..periodSeconds).
int totpSecondsRemaining(std::int64_t unixTime, int periodSeconds = 30) noexcept;

} // namespace pm::crypto
