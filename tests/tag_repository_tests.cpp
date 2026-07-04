#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>

#include "core/crypto/Crypto.h"
#include "core/crypto/SecureBuffer.h"
#include "core/model/Entry.h"
#include "core/storage/Database.h"
#include "core/storage/EntryRepository.h"
#include "core/storage/Migrations.h"
#include "core/storage/TagRepository.h"

using namespace pm;
using pm::model::Entry;
using pm::model::EntryType;
using pm::storage::Database;
using pm::storage::EntryRepository;
using pm::storage::TagRepository;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_tag;

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

std::int64_t addEntry(Database &db)
{
    Entry e;
    e.type = EntryType::Login;
    e.title = "Item";
    e.createdAt = 1;
    e.updatedAt = 1;
    return EntryRepository(db).add(e);
}

} // namespace

TEST_CASE("TagRepository getOrCreate de-duplicates by name", "[storage][tag]")
{
    const auto path = freshDbPath("pm_test_tag_dedup.db");
    {
        Database db = Database::open(path.string(), makeDek(0xA0));
        storage::migrate(db);
        TagRepository repo(db);

        const auto first = repo.getOrCreate("work");
        const auto again = repo.getOrCreate("work"); // same name -> same id
        REQUIRE(first == again);
        REQUIRE(repo.list().size() == 1);

        repo.getOrCreate("personal");
        REQUIRE(repo.list().size() == 2);
        REQUIRE(repo.getByName("work").has_value());
        REQUIRE_FALSE(repo.getByName("missing").has_value());
    }
    std::filesystem::remove(path);
}

TEST_CASE("TagRepository assigns and unassigns tags to an entry", "[storage][tag]")
{
    const auto path = freshDbPath("pm_test_tag_assign.db");
    {
        Database db = Database::open(path.string(), makeDek(0xA1));
        storage::migrate(db);
        TagRepository repo(db);

        const auto entryId = addEntry(db);
        const auto work = repo.getOrCreate("work");
        const auto urgent = repo.getOrCreate("urgent");

        repo.assign(entryId, work);
        repo.assign(entryId, urgent);
        repo.assign(entryId, work); // idempotent, no duplicate

        auto tags = repo.tagsForEntry(entryId);
        REQUIRE(tags.size() == 2);
        REQUIRE(tags[0].name == "urgent"); // ordered by name
        REQUIRE(tags[1].name == "work");

        repo.unassign(entryId, urgent);
        tags = repo.tagsForEntry(entryId);
        REQUIRE(tags.size() == 1);
        REQUIRE(tags[0].name == "work");
    }
    std::filesystem::remove(path);
}

TEST_CASE("TagRepository remove cascades its entry links", "[storage][tag]")
{
    const auto path = freshDbPath("pm_test_tag_remove.db");
    {
        Database db = Database::open(path.string(), makeDek(0xA2));
        storage::migrate(db);
        TagRepository repo(db);

        const auto entryId = addEntry(db);
        const auto tagId = repo.getOrCreate("work");
        repo.assign(entryId, tagId);
        REQUIRE(repo.tagsForEntry(entryId).size() == 1);

        REQUIRE(repo.remove(tagId));
        REQUIRE(repo.tagsForEntry(entryId).empty()); // link cascaded away
    }
    std::filesystem::remove(path);
}
