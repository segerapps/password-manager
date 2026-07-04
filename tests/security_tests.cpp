// Adversarial regression tests for the security fixes from the 2026-06-10 review:
//   S1 — integer-overflow / malformed .pmbackup must be rejected, not OOB-read.
//   S2 — absurd / tampered KDF parameters in vault.meta must be rejected.
//   S6 — unlock with a missing vault.db must error, not silently create an empty one.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <sodium.h>

#include "core/crypto/Crypto.h"
#include "core/vault/Backup.h"
#include "core/vault/VaultMeta.h"
#include "core/vault/VaultSession.h"

using namespace pm;
using pm::vault::VaultSession;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_security;

std::filesystem::path freshDir(const char *name)
{
    auto p = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    return p;
}

std::filesystem::path tmpFile(const char *name)
{
    auto p = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p;
}

template <class T>
void appendLE(std::vector<unsigned char> &b, T value)
{
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        b.push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xFF));
    }
}

// Build a .pmbackup blob with the given header fields (so we can craft hostile ones).
std::vector<unsigned char> makeBackup(const char *magic4, std::uint32_t version,
                                      std::uint64_t metaLen, std::uint64_t dbLen,
                                      const std::vector<unsigned char> &body)
{
    std::vector<unsigned char> b(magic4, magic4 + 4);
    appendLE<std::uint32_t>(b, version);
    appendLE<std::uint64_t>(b, metaLen);
    appendLE<std::uint64_t>(b, dbLen);
    b.insert(b.end(), body.begin(), body.end());
    return b;
}

void writeBytes(const std::filesystem::path &p, const std::vector<unsigned char> &b)
{
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(b.data()), static_cast<std::streamsize>(b.size()));
}

std::string b64(const std::vector<unsigned char> &v)
{
    const std::size_t cap = sodium_base64_encoded_len(v.size(), sodium_base64_VARIANT_ORIGINAL);
    std::string out(cap, '\0');
    sodium_bin2base64(out.data(), cap, v.data(), v.size(), sodium_base64_VARIANT_ORIGINAL);
    out.resize(std::strlen(out.c_str()));
    return out;
}

// A vault.meta JSON with valid base64 placeholders; individual fields can be made
// hostile to exercise the validation in VaultMeta::fromJson.
std::string makeMetaJson(const std::string &format, const std::string &kdfAlgo,
                         long long memKib, long long iterations, const std::string &wrapAlgo)
{
    const std::string salt = b64(std::vector<unsigned char>(16, 0));
    const std::string nonce = b64(std::vector<unsigned char>(24, 0));
    const std::string wrapped = b64(std::vector<unsigned char>(48, 0));
    return std::string("{\"format\":\"") + format
        + "\",\"schema_version\":1,\"app_version\":\"0.1.0\",\"created_at\":1000,"
        + "\"kdf\":{\"algo\":\"" + kdfAlgo + "\",\"salt_b64\":\"" + salt
        + "\",\"mem_kib\":" + std::to_string(memKib) + ",\"iterations\":"
        + std::to_string(iterations) + ",\"parallelism\":1},"
        + "\"wrap\":{\"algo\":\"" + wrapAlgo + "\",\"nonce_b64\":\"" + nonce
        + "\",\"wrapped_dek_b64\":\"" + wrapped + "\"}}";
}

} // namespace

// ── S1 — malformed / overflow .pmbackup must be rejected ─────────────────────
TEST_CASE("importBackup rejects malformed backups", "[security][S1][backup]")
{
    const auto dst = freshDir("pm_sec_s1_dst");
    const auto file = tmpFile("pm_sec_s1.pmbackup");

    SECTION("bad magic")
    {
        writeBytes(file, makeBackup("XXXX", 1, 0, 0, {}));
        REQUIRE_THROWS(vault::importBackup(file.string(), dst.string(), false));
    }
    SECTION("too small for a header")
    {
        writeBytes(file, {1, 2, 3});
        REQUIRE_THROWS(vault::importBackup(file.string(), dst.string(), false));
    }
    SECTION("unsupported version")
    {
        writeBytes(file, makeBackup("PMBK", 999, 0, 0, {}));
        REQUIRE_THROWS(vault::importBackup(file.string(), dst.string(), false));
    }
    SECTION("truncated body")
    {
        writeBytes(file, makeBackup("PMBK", 1, 100, 100, {})); // claims 200 bytes, has none
        REQUIRE_THROWS(vault::importBackup(file.string(), dst.string(), false));
    }
    SECTION("integer-overflow lengths (the S1 vector)")
    {
        // metaLen + dbLen would wrap to a tiny value under the old check.
        writeBytes(file, makeBackup("PMBK", 1, 0xFFFFFFFFFFFFFF00ULL, 0x200, {}));
        REQUIRE_THROWS(vault::importBackup(file.string(), dst.string(), false));
    }

    // No vault should have been written from any rejected backup.
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(dst) / "vault.db"));

    std::error_code ec;
    std::filesystem::remove(file, ec);
    std::filesystem::remove_all(dst, ec);
}

// ── S2 — absurd / tampered KDF parameters in vault.meta must be rejected ──────
TEST_CASE("VaultMeta::fromJson validates format and KDF parameters", "[security][S2][vault]")
{
    // Baseline: a well-formed meta parses fine.
    REQUIRE_NOTHROW(
        vault::VaultMeta::fromJson(makeMetaJson("pmvault", "argon2id", 262144, 3, "xchacha20poly1305")));

    // Hostile variants all throw.
    REQUIRE_THROWS(vault::VaultMeta::fromJson(makeMetaJson("nope", "argon2id", 262144, 3, "xchacha20poly1305")));
    REQUIRE_THROWS(vault::VaultMeta::fromJson(makeMetaJson("pmvault", "scrypt", 262144, 3, "xchacha20poly1305")));
    REQUIRE_THROWS(vault::VaultMeta::fromJson(makeMetaJson("pmvault", "argon2id", 262144, 3, "aes256gcm")));
    REQUIRE_THROWS(vault::VaultMeta::fromJson(makeMetaJson("pmvault", "argon2id", 2000000, 3, "xchacha20poly1305"))); // ~2 GiB
    REQUIRE_THROWS(vault::VaultMeta::fromJson(makeMetaJson("pmvault", "argon2id", 1, 3, "xchacha20poly1305")));       // 1 KiB
    REQUIRE_THROWS(vault::VaultMeta::fromJson(makeMetaJson("pmvault", "argon2id", 262144, 100, "xchacha20poly1305"))); // ops too high
    REQUIRE_THROWS(vault::VaultMeta::fromJson(makeMetaJson("pmvault", "argon2id", 262144, 0, "xchacha20poly1305")));   // ops too low
}

// ── S6 — unlock with a missing vault.db must error, not create an empty vault ─
TEST_CASE("unlock with a missing vault.db errors instead of an empty vault", "[security][S6][vault]")
{
    const auto dir = freshDir("pm_sec_s6");
    { VaultSession::create(dir.string(), "master-pw"); } // create then close on scope exit

    // Delete the database but keep vault.meta (simulates loss/corruption).
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(dir) / "vault.db", ec);
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(dir) / "vault.db"));

    // The right password gets past key-unwrap, but the open must fail (no silent create).
    REQUIRE_THROWS(VaultSession::unlock(dir.string(), "master-pw"));
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(dir) / "vault.db"));

    std::filesystem::remove_all(dir, ec);
}
