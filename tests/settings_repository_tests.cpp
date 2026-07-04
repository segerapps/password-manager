#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>

#include "core/crypto/Crypto.h"
#include "core/crypto/SecureBuffer.h"
#include "core/storage/Database.h"
#include "core/storage/Migrations.h"
#include "core/storage/SettingsRepository.h"

using namespace pm;
using pm::storage::Database;
using pm::storage::SettingsRepository;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_settings;

crypto::SecureBuffer makeDek(std::uint8_t fill)
{
    crypto::SecureBuffer dek(crypto::kKeySize);
    std::memset(dek.data(), fill, dek.size());
    return dek;
}

std::filesystem::path freshDbPath(const char *name)
{
    auto p = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p;
}

} // namespace

TEST_CASE("SettingsRepository set/get and defaults", "[storage][settings]")
{
    const auto path = freshDbPath("pm_test_settings.db");
    {
        Database db = Database::open(path.string(), makeDek(0xC0));
        storage::migrate(db);
        SettingsRepository repo(db);

        REQUIRE_FALSE(repo.get("missing").has_value());
        REQUIRE(repo.getInt("auto_lock_seconds", 180) == 180); // fallback

        repo.set("auto_lock_seconds", "300");
        REQUIRE(repo.get("auto_lock_seconds").value() == "300");
        REQUIRE(repo.getInt("auto_lock_seconds", 180) == 300);

        repo.set("auto_lock_seconds", "60"); // overwrite
        REQUIRE(repo.getInt("auto_lock_seconds", 180) == 60);
    }
    std::filesystem::remove(path);
}
