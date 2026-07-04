#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>

#include "core/crypto/Crypto.h"
#include "core/crypto/SecureBuffer.h"
#include "core/model/Folder.h"
#include "core/storage/Database.h"
#include "core/storage/FolderRepository.h"
#include "core/storage/Migrations.h"

using namespace pm;
using pm::model::Folder;
using pm::storage::Database;
using pm::storage::FolderRepository;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_folder;

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

TEST_CASE("FolderRepository add + getById round-trips", "[storage][folder]")
{
    const auto path = freshDbPath("pm_test_folder_rt.db");
    {
        Database db = Database::open(path.string(), makeDek(0x90));
        storage::migrate(db);
        FolderRepository repo(db);

        Folder f;
        f.name = "Work";
        f.createdAt = 100;

        const auto id = repo.add(f);
        REQUIRE(id > 0);

        const auto loaded = repo.getById(id);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->name == "Work");
        REQUIRE_FALSE(loaded->parentId.has_value());
        REQUIRE(loaded->createdAt == 100);
    }
    std::filesystem::remove(path);
}

TEST_CASE("FolderRepository list is ordered by name", "[storage][folder]")
{
    const auto path = freshDbPath("pm_test_folder_list.db");
    {
        Database db = Database::open(path.string(), makeDek(0x91));
        storage::migrate(db);
        FolderRepository repo(db);

        Folder beta;
        beta.name = "Beta";
        Folder alpha;
        alpha.name = "Alpha";
        repo.add(beta);
        repo.add(alpha);

        const auto all = repo.list();
        REQUIRE(all.size() == 2);
        REQUIRE(all[0].name == "Alpha");
        REQUIRE(all[1].name == "Beta");
    }
    std::filesystem::remove(path);
}

TEST_CASE("FolderRepository remove nulls a child's parentId", "[storage][folder]")
{
    const auto path = freshDbPath("pm_test_folder_setnull.db");
    {
        Database db = Database::open(path.string(), makeDek(0x92));
        storage::migrate(db);
        FolderRepository repo(db);

        Folder parent;
        parent.name = "Parent";
        const auto parentId = repo.add(parent);

        Folder child;
        child.name = "Child";
        child.parentId = parentId;
        const auto childId = repo.add(child);

        REQUIRE(repo.getById(childId)->parentId == parentId);

        // ON DELETE SET NULL: deleting the parent must orphan (not delete) the child.
        REQUIRE(repo.remove(parentId));
        const auto reloaded = repo.getById(childId);
        REQUIRE(reloaded.has_value());
        REQUIRE_FALSE(reloaded->parentId.has_value());
    }
    std::filesystem::remove(path);
}
