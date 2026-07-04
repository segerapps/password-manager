#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <sodium.h>
#include <sqlcipher/sqlite3.h>

#include "core/crypto/Crypto.h"
#include "core/crypto/SecureBuffer.h"
#include "core/storage/Database.h"

using namespace pm;
using pm::storage::Database;
using pm::storage::DbError;

namespace {

// libsodium must be initialised before SecureBuffer/randombytes are touched.
const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium;

// A deterministic 32-byte DEK so tests are reproducible; `fill` distinguishes a
// "right" key from a "wrong" one.
crypto::SecureBuffer makeDek(std::uint8_t fill)
{
    crypto::SecureBuffer dek(crypto::kKeySize);
    std::memset(dek.data(), fill, dek.size());
    return dek;
}

// A temp path in the OS temp dir, with any stale copy removed first.
std::filesystem::path freshDbPath(const char *name)
{
    auto p = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p;
}

std::vector<unsigned char> readAllBytes(const std::filesystem::path &p)
{
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("Database round-trips data with the correct DEK", "[storage][db]")
{
    const auto path = freshDbPath("pm_test_roundtrip.db");
    const std::string secret = "super-secret-passphrase-12345";

    {
        Database db = Database::open(path.string(), makeDek(0xA1));
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT);");
        db.exec("INSERT INTO t (v) VALUES ('" + secret + "');");
    } // handle closed here

    {
        Database db = Database::open(path.string(), makeDek(0xA1)); // same key
        sqlite3_stmt *stmt = nullptr;
        REQUIRE(sqlite3_prepare_v2(db.handle(), "SELECT v FROM t WHERE id = 1;", -1,
                                   &stmt, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
        const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        REQUIRE(std::string(text) == secret);
        sqlite3_finalize(stmt);
    }

    std::filesystem::remove(path);
}

TEST_CASE("Database rejects the wrong DEK", "[storage][db]")
{
    const auto path = freshDbPath("pm_test_wrongkey.db");

    {
        Database db = Database::open(path.string(), makeDek(0x11));
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY);");
    }

    // Opening the existing encrypted file with a different key must fail.
    REQUIRE_THROWS_AS(Database::open(path.string(), makeDek(0x22)), DbError);

    std::filesystem::remove(path);
}

TEST_CASE("Database file is encrypted at rest", "[storage][db][encryption]")
{
    const auto path = freshDbPath("pm_test_encrypted.db");
    const std::string secret = "plaintext-should-not-appear-on-disk";

    {
        Database db = Database::open(path.string(), makeDek(0x5C));
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT);");
        db.exec("INSERT INTO t (v) VALUES ('" + secret + "');");
    }

    const std::vector<unsigned char> bytes = readAllBytes(path);
    REQUIRE_FALSE(bytes.empty());

    // A normal (unencrypted) SQLite file begins with the magic "SQLite format 3".
    // A SQLCipher file starts with a random salt instead, so this must NOT match.
    const std::string magic = "SQLite format 3";
    const bool hasSqliteHeader =
        bytes.size() >= magic.size() &&
        std::memcmp(bytes.data(), magic.data(), magic.size()) == 0;
    REQUIRE_FALSE(hasSqliteHeader);

    // And the plaintext secret must not be findable anywhere in the file.
    const auto found =
        std::search(bytes.begin(), bytes.end(), secret.begin(), secret.end());
    REQUIRE(found == bytes.end());

    std::filesystem::remove(path);
}
