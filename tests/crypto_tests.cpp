#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>

#include <sodium.h>

#include "core/crypto/Crypto.h"
#include "core/crypto/SecureBuffer.h"

using namespace pm::crypto;

namespace {

// The real default KDF (~256 MiB) is deliberately slow. For unit tests we use
// libsodium's INTERACTIVE preset — valid Argon2id params, but fast.
KdfParams fastParams(std::uint8_t saltFill = 0x42)
{
    KdfParams p;
    p.salt.fill(saltFill);
    p.opslimit = crypto_pwhash_OPSLIMIT_INTERACTIVE;
    p.memlimit = crypto_pwhash_MEMLIMIT_INTERACTIVE;
    return p;
}

bool sameKey(const SecureBuffer &a, const SecureBuffer &b)
{
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()) == 0;
}

// Ensure sodium_init() runs before any test body executes.
const struct CryptoInit
{
    CryptoInit() { init(); }
} g_init;

} // namespace

TEST_CASE("SecureBuffer move semantics and zero-size", "[crypto][securebuffer]")
{
    SecureBuffer a(32);
    REQUIRE(a.size() == 32);
    REQUIRE(a.data() != nullptr);
    std::memset(a.data(), 0xAB, a.size());

    SecureBuffer b = std::move(a);
    REQUIRE(b.size() == 32);
    REQUIRE(b.data()[0] == 0xAB);
    REQUIRE(a.size() == 0);    // NOLINT(bugprone-use-after-move) — checking moved-from state
    REQUIRE(a.data() == nullptr);

    SecureBuffer empty;
    REQUIRE(empty.empty());
    REQUIRE(empty.size() == 0);
}

TEST_CASE("deriveKek is deterministic and salt-sensitive", "[crypto][kdf]")
{
    const std::string pw = "correct horse battery staple";

    SecureBuffer k1 = deriveKek(pw, fastParams(0x01));
    SecureBuffer k2 = deriveKek(pw, fastParams(0x01));
    SecureBuffer k3 = deriveKek(pw, fastParams(0x02));

    REQUIRE(k1.size() == kKeySize);
    REQUIRE(sameKey(k1, k2));        // same password + salt -> same KEK
    REQUIRE_FALSE(sameKey(k1, k3));  // different salt -> different KEK
}

TEST_CASE("deriveKek is password-sensitive", "[crypto][kdf]")
{
    const KdfParams p = fastParams();
    SecureBuffer a = deriveKek("password-A", p);
    SecureBuffer b = deriveKek("password-B", p);
    REQUIRE_FALSE(sameKey(a, b));
}

TEST_CASE("generateDek yields fresh 32-byte keys", "[crypto][dek]")
{
    SecureBuffer a = generateDek();
    SecureBuffer b = generateDek();
    REQUIRE(a.size() == kKeySize);
    REQUIRE_FALSE(sameKey(a, b));
}

TEST_CASE("wrap/unwrap round trip recovers the DEK", "[crypto][wrap]")
{
    SecureBuffer kek = deriveKek("master", fastParams());
    SecureBuffer dek = generateDek();

    WrappedKey w = wrapDek(dek, kek);
    auto opened = unwrapDek(w, kek);

    REQUIRE(opened.has_value());
    REQUIRE(sameKey(*opened, dek));
}

TEST_CASE("unwrap fails with the wrong KEK", "[crypto][wrap]")
{
    SecureBuffer kek = deriveKek("master", fastParams(0x10));
    SecureBuffer dek = generateDek();
    WrappedKey w = wrapDek(dek, kek);

    SecureBuffer wrong = deriveKek("not-the-master", fastParams(0x10));
    REQUIRE_FALSE(unwrapDek(w, wrong).has_value());
}

TEST_CASE("unwrap fails on tampered ciphertext or nonce", "[crypto][wrap]")
{
    SecureBuffer kek = deriveKek("master", fastParams());
    SecureBuffer dek = generateDek();

    WrappedKey w = wrapDek(dek, kek);
    SECTION("flip a ciphertext bit")
    {
        w.ciphertext[0] ^= 0x01;
        REQUIRE_FALSE(unwrapDek(w, kek).has_value());
    }
    SECTION("flip a nonce bit")
    {
        w.nonce[0] ^= 0x01;
        REQUIRE_FALSE(unwrapDek(w, kek).has_value());
    }
}

TEST_CASE("createKeyMaterial + openVaultKey round trip", "[crypto][vault]")
{
    // Uses the real default KDF (one slow call is fine).
    const std::string pw = "S3cure-Master-Pass!";
    KeyMaterial km = createKeyMaterial(pw);

    REQUIRE(km.dek.size() == kKeySize);

    auto opened = openVaultKey(pw, km.kdf, km.wrapped);
    REQUIRE(opened.has_value());
    REQUIRE(sameKey(*opened, km.dek));

    REQUIRE_FALSE(openVaultKey("wrong-password", km.kdf, km.wrapped).has_value());
}

TEST_CASE("changing the master password rewraps the same DEK", "[crypto][vault]")
{
    // The wrapped-DEK design lets us change the password WITHOUT re-encrypting
    // the database: only the wrapping of the (unchanged) DEK is replaced.
    SecureBuffer dek = generateDek();

    SecureBuffer oldKek = deriveKek("old-pass", fastParams(0x33));
    WrappedKey wOld = wrapDek(dek, oldKek);

    SecureBuffer newKek = deriveKek("new-pass", fastParams(0x44));
    WrappedKey wNew = wrapDek(dek, newKek);

    REQUIRE_FALSE(unwrapDek(wNew, oldKek).has_value()); // old password no longer works
    auto opened = unwrapDek(wNew, newKek);
    REQUIRE(opened.has_value());
    REQUIRE(sameKey(*opened, dek));                     // same DEK -> DB stays readable
}
