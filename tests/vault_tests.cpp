#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/crypto/Crypto.h"
#include "core/model/Entry.h"
#include "core/storage/EntryRepository.h"
#include "core/vault/VaultSession.h"

using namespace pm;
using pm::model::Entry;
using pm::model::EntryType;
using pm::model::Field;
using pm::model::FieldType;
using pm::storage::EntryRepository;
using pm::vault::VaultError;
using pm::vault::VaultSession;
using pm::vault::WrongPassword;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_vault;

std::filesystem::path freshVaultDir(const char *name)
{
    auto p = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    return p;
}

Entry sampleLogin()
{
    Entry e;
    e.type = EntryType::Login;
    e.title = "GitHub";
    e.createdAt = 1000;
    e.updatedAt = 1000;
    e.fields = {Field{0, "password", "Password", "s3cr3t!", FieldType::Secret, true, 0}};
    return e;
}

} // namespace

TEST_CASE("Vault create then unlock round-trips data", "[vault]")
{
    const auto dir = freshVaultDir("pm_vault_rt");
    std::int64_t id = 0;
    {
        VaultSession s = VaultSession::create(dir.string(), "master-pw");
        REQUIRE(s.isUnlocked());
        EntryRepository repo(s.database());
        id = repo.add(sampleLogin());
        REQUIRE(id > 0);
        s.lock();
        REQUIRE_FALSE(s.isUnlocked());
    }
    {
        VaultSession s = VaultSession::unlock(dir.string(), "master-pw");
        EntryRepository repo(s.database());
        const auto loaded = repo.getById(id);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->title == "GitHub");
        REQUIRE(loaded->fields.at(0).value == "s3cr3t!");
    }
    std::filesystem::remove_all(dir);
}

TEST_CASE("Vault unlock rejects the wrong password", "[vault]")
{
    const auto dir = freshVaultDir("pm_vault_wrongpw");
    { VaultSession::create(dir.string(), "correct-pw"); } // create, then close on scope exit

    REQUIRE_THROWS_AS(VaultSession::unlock(dir.string(), "WRONG-pw"), WrongPassword);

    std::filesystem::remove_all(dir);
}

TEST_CASE("Vault create refuses an existing vault", "[vault]")
{
    const auto dir = freshVaultDir("pm_vault_exists");
    { VaultSession::create(dir.string(), "pw"); }

    REQUIRE_THROWS_AS(VaultSession::create(dir.string(), "pw2"), VaultError);

    std::filesystem::remove_all(dir);
}

TEST_CASE("Vault change master password swaps the password but keeps data", "[vault]")
{
    const auto dir = freshVaultDir("pm_vault_chpw");
    std::int64_t id = 0;
    {
        VaultSession s = VaultSession::create(dir.string(), "old-pw");
        EntryRepository repo(s.database());
        id = repo.add(sampleLogin());
        s.changeMasterPassword("new-pw");
    }

    // The old password no longer unlocks the vault.
    REQUIRE_THROWS_AS(VaultSession::unlock(dir.string(), "old-pw"), WrongPassword);

    // The new password works, and the data survived (no DB re-encryption).
    {
        VaultSession s = VaultSession::unlock(dir.string(), "new-pw");
        EntryRepository repo(s.database());
        REQUIRE(repo.getById(id).has_value());
    }
    std::filesystem::remove_all(dir);
}
