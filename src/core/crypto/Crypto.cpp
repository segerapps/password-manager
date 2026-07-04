#include "core/crypto/Crypto.h"

#include <stdexcept>

#include <sodium.h>

namespace pm::crypto {

// Compile-time sanity: our sizes must match libsodium's, or wrap/unwrap and the
// vault.meta layout would silently disagree.
static_assert(kKeySize == crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
static_assert(kSaltSize == crypto_pwhash_SALTBYTES);
static_assert(kNonceSize == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
static_assert(kTagSize == crypto_aead_xchacha20poly1305_ietf_ABYTES);

namespace {
// Domain-separation tag bound as AEAD additional data, so a wrapped DEK can
// never be confused with ciphertext from some other context/version.
constexpr unsigned char kAad[] = {'p', 'm', '/', 'd', 'e', 'k', '/', 'v', '1'};
constexpr std::size_t kAadLen = sizeof(kAad);
} // namespace

void init()
{
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialization failed");
    }
}

KdfParams KdfParams::createDefault()
{
    KdfParams p;
    randombytes_buf(p.salt.data(), p.salt.size());
    p.opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
    p.memlimit = crypto_pwhash_MEMLIMIT_MODERATE; // ~256 MiB
    return p;
}

SecureBuffer deriveKek(std::string_view password, const KdfParams &params)
{
    SecureBuffer kek(kKeySize);
    const int rc = crypto_pwhash(kek.data(),
                                 kKeySize,
                                 password.data(),
                                 password.size(),
                                 params.salt.data(),
                                 params.opslimit,
                                 static_cast<std::size_t>(params.memlimit),
                                 crypto_pwhash_ALG_ARGON2ID13);
    if (rc != 0) {
        throw std::runtime_error(
            "Argon2id key derivation failed (out of memory for the requested cost?)");
    }
    return kek;
}

SecureBuffer generateDek()
{
    SecureBuffer dek(kKeySize);
    randombytes_buf(dek.data(), kKeySize);
    return dek;
}

WrappedKey wrapDek(const SecureBuffer &dek, const SecureBuffer &kek)
{
    if (dek.size() != kKeySize || kek.size() != kKeySize) {
        throw std::invalid_argument("wrapDek: keys must be 32 bytes");
    }

    WrappedKey out;
    randombytes_buf(out.nonce.data(), out.nonce.size());

    unsigned long long clen = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(out.ciphertext.data(),
                                                &clen,
                                                dek.data(),
                                                dek.size(),
                                                kAad,
                                                kAadLen,
                                                nullptr, // nsec (unused)
                                                out.nonce.data(),
                                                kek.data());
    if (clen != kWrappedKeySize) {
        throw std::runtime_error("wrapDek: unexpected ciphertext length");
    }
    return out;
}

std::optional<SecureBuffer> unwrapDek(const WrappedKey &wrapped, const SecureBuffer &kek)
{
    if (kek.size() != kKeySize) {
        throw std::invalid_argument("unwrapDek: KEK must be 32 bytes");
    }

    SecureBuffer dek(kKeySize);
    unsigned long long mlen = 0;
    const int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(dek.data(),
                                                              &mlen,
                                                              nullptr, // nsec (unused)
                                                              wrapped.ciphertext.data(),
                                                              wrapped.ciphertext.size(),
                                                              kAad,
                                                              kAadLen,
                                                              wrapped.nonce.data(),
                                                              kek.data());
    if (rc != 0) {
        return std::nullopt; // wrong password or tampered data
    }
    return dek;
}

KeyMaterial createKeyMaterial(std::string_view password)
{
    KeyMaterial km;
    km.kdf = KdfParams::createDefault();
    SecureBuffer kek = deriveKek(password, km.kdf);
    km.dek = generateDek();
    km.wrapped = wrapDek(km.dek, kek);
    return km;
}

std::optional<SecureBuffer> openVaultKey(std::string_view password,
                                         const KdfParams &params,
                                         const WrappedKey &wrapped)
{
    SecureBuffer kek = deriveKek(password, params);
    return unwrapDek(wrapped, kek);
}

} // namespace pm::crypto
