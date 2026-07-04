#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/crypto/Crypto.h"
#include "core/model/Entry.h"
#include "core/storage/EntryRepository.h"
#include "core/vault/Backup.h"
#include "core/vault/VaultSession.h"

using namespace pm;
using pm::model::Entry;
using pm::model::EntryType;
using pm::model::Field;
using pm::model::FieldType;
using pm::storage::EntryRepository;
using pm::vault::VaultSession;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_backup;

std::filesystem::path freshDir(const char *name)
{
    auto p = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    return p;
}

std::int64_t seedVault(const std::string &dir, const std::string &password)
{
    VaultSession s = VaultSession::create(dir, password);
    EntryRepository repo(s.database());
    Entry e;
    e.type = EntryType::Login;
    e.title = "GitHub";
    e.createdAt = 1;
    e.updatedAt = 1;
    e.fields = {Field{0, "password", "Password", "s3cr3t!", FieldType::Secret, true, 0}};
    return repo.add(e);
}

} // namespace

TEST_CASE("Backup export then import recreates the vault elsewhere", "[vault][backup]")
{
    const auto srcDir = freshDir("pm_bk_src");
    const auto dstDir = freshDir("pm_bk_dst");
    const auto backup = (std::filesystem::temp_directory_path() / "pm_test.pmbackup").string();
    std::error_code ec;
    std::filesystem::remove(backup, ec);

    std::int64_t id = 0;
    {
        id = seedVault(srcDir.string(), "master-pw"); // create + add, then close on scope exit
    }

    vault::exportBackup(srcDir.string(), backup);
    REQUIRE(std::filesystem::exists(backup));

    // Restore into a different, empty directory — like a fresh PC.
    vault::importBackup(backup, dstDir.string(), /*overwrite=*/false);

    {
        VaultSession s = VaultSession::unlock(dstDir.string(), "master-pw");
        EntryRepository repo(s.database());
        const auto loaded = repo.getById(id);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->title == "GitHub");
        REQUIRE(loaded->fields.at(0).value == "s3cr3t!");
    }

    std::filesystem::remove_all(srcDir);
    std::filesystem::remove_all(dstDir);
    std::filesystem::remove(backup, ec);
}

TEST_CASE("Backup restored vault still needs the right password", "[vault][backup]")
{
    const auto srcDir = freshDir("pm_bk_src2");
    const auto dstDir = freshDir("pm_bk_dst2");
    const auto backup = (std::filesystem::temp_directory_path() / "pm_test2.pmbackup").string();
    std::error_code ec;
    std::filesystem::remove(backup, ec);

    { seedVault(srcDir.string(), "correct-pw"); }
    vault::exportBackup(srcDir.string(), backup);
    vault::importBackup(backup, dstDir.string(), false);

    REQUIRE_THROWS_AS(VaultSession::unlock(dstDir.string(), "WRONG-pw"),
                      pm::vault::WrongPassword);

    std::filesystem::remove_all(srcDir);
    std::filesystem::remove_all(dstDir);
    std::filesystem::remove(backup, ec);
}

TEST_CASE("Backup import refuses to overwrite an existing vault", "[vault][backup]")
{
    const auto srcDir = freshDir("pm_bk_src3");
    const auto dstDir = freshDir("pm_bk_dst3");
    const auto backup = (std::filesystem::temp_directory_path() / "pm_test3.pmbackup").string();
    std::error_code ec;
    std::filesystem::remove(backup, ec);

    { seedVault(srcDir.string(), "pw"); }
    vault::exportBackup(srcDir.string(), backup);

    { seedVault(dstDir.string(), "other-pw"); } // dst already has a vault

    REQUIRE_THROWS(vault::importBackup(backup, dstDir.string(), /*overwrite=*/false));

    std::filesystem::remove_all(srcDir);
    std::filesystem::remove_all(dstDir);
    std::filesystem::remove(backup, ec);
}
