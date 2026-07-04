#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "core/crypto/SecureBuffer.h"

// Crypto core for the wrapped-DEK key hierarchy (see docs/DATA-MODEL.md §3.2):
//
//   master password --Argon2id(salt)--> KEK --unwrap--> DEK --> SQLCipher key
//
// Nothing here persists secrets. The KdfParams + WrappedKey are the only things
// written to vault.meta (plaintext); the DEK lives only in a SecureBuffer while
// the vault is unlocked.
namespace pm::crypto {

// Call once at startup before any other function here. Idempotent and
// thread-safe after the first call. Throws std::runtime_error on failure.
void init();

inline constexpr std::size_t kKeySize = 32;        // DEK / KEK (256-bit)
inline constexpr std::size_t kSaltSize = 16;       // Argon2id salt
inline constexpr std::size_t kNonceSize = 24;      // XChaCha20-Poly1305 nonce
inline constexpr std::size_t kTagSize = 16;        // Poly1305 auth tag
inline constexpr std::size_t kWrappedKeySize = kKeySize + kTagSize; // 48

// Argon2id parameters. Stored in vault.meta (plaintext — salt and params are
// not secret). `opslimit`/`memlimit` follow libsodium's crypto_pwhash units.
struct KdfParams
{
    std::array<std::uint8_t, kSaltSize> salt{};
    std::uint64_t opslimit = 0; // iterations
    std::uint64_t memlimit = 0; // bytes

    // Fresh random salt + recommended cost (~256 MiB, moderate). Used when
    // creating a new vault or rotating the master password.
    static KdfParams createDefault();
};

// A DEK encrypted under a KEK with XChaCha20-Poly1305 (nonce + ciphertext+tag).
struct WrappedKey
{
    std::array<std::uint8_t, kNonceSize> nonce{};
    std::array<std::uint8_t, kWrappedKeySize> ciphertext{};
};

// Everything produced when a vault is created. `kdf` + `wrapped` go to
// vault.meta; `dek` is the live database key and is never persisted.
struct KeyMaterial
{
    KdfParams kdf;
    WrappedKey wrapped;
    SecureBuffer dek;
};

// Derive a 32-byte KEK from the master password via Argon2id.
// Throws std::runtime_error if derivation fails (typically out of memory).
SecureBuffer deriveKek(std::string_view password, const KdfParams &params);

// Generate a fresh random 32-byte DEK (the SQLCipher key).
SecureBuffer generateDek();

// Wrap / unwrap the DEK under a KEK. unwrap returns std::nullopt on
// authentication failure — i.e. wrong password or tampered data.
WrappedKey wrapDek(const SecureBuffer &dek, const SecureBuffer &kek);
std::optional<SecureBuffer> unwrapDek(const WrappedKey &wrapped, const SecureBuffer &kek);

// High-level convenience tying the pieces together:
//   createKeyMaterial — new vault: random salt + DEK, wrapped under the password.
//   openVaultKey      — unlock: re-derive KEK, unwrap DEK (nullopt if wrong pw).
KeyMaterial createKeyMaterial(std::string_view password);
std::optional<SecureBuffer> openVaultKey(std::string_view password,
                                         const KdfParams &params,
                                         const WrappedKey &wrapped);

} // namespace pm::crypto
