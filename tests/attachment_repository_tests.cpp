#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

#include "core/crypto/Crypto.h"
#include "core/crypto/SecureBuffer.h"
#include "core/model/Attachment.h"
#include "core/model/Entry.h"
#include "core/storage/AttachmentRepository.h"
#include "core/storage/Database.h"
#include "core/storage/EntryRepository.h"
#include "core/storage/Migrations.h"

using namespace pm;
using pm::model::Attachment;
using pm::model::Entry;
using pm::model::EntryType;
using pm::storage::AttachmentRepository;
using pm::storage::Database;
using pm::storage::EntryRepository;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_attach;

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

Attachment samplePng(std::int64_t entryId)
{
    Attachment a;
    a.entryId = entryId;
    a.filename = "shot.png";
    a.mimeType = "image/png";
    a.createdAt = 500;
    a.data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 1, 2, 3, 4, 5};
    return a;
}

} // namespace

TEST_CASE("AttachmentRepository add + getById round-trips the blob", "[storage][attachment]")
{
    const auto path = freshDbPath("pm_test_attach_rt.db");
    {
        Database db = Database::open(path.string(), makeDek(0xB0));
        storage::migrate(db);
        AttachmentRepository repo(db);

        const auto entryId = addEntry(db);
        const auto in = samplePng(entryId);
        const auto id = repo.add(in);
        REQUIRE(id > 0);

        const auto loaded = repo.getById(id);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->filename == "shot.png");
        REQUIRE(loaded->mimeType == "image/png");
        REQUIRE(loaded->byteSize == static_cast<std::int64_t>(in.data.size()));
        REQUIRE(loaded->data == in.data); // exact bytes survive encryption round-trip
    }
    std::filesystem::remove(path);
}

TEST_CASE("AttachmentRepository can skip the blob", "[storage][attachment]")
{
    const auto path = freshDbPath("pm_test_attach_nodata.db");
    {
        Database db = Database::open(path.string(), makeDek(0xB1));
        storage::migrate(db);
        AttachmentRepository repo(db);

        const auto entryId = addEntry(db);
        const auto id = repo.add(samplePng(entryId));

        const auto meta = repo.getById(id, /*withData=*/false);
        REQUIRE(meta.has_value());
        REQUIRE(meta->byteSize > 0);
        REQUIRE(meta->data.empty()); // metadata only

        const auto list = repo.listForEntry(entryId);
        REQUIRE(list.size() == 1);
        REQUIRE(list[0].data.empty());
        REQUIRE(list[0].filename == "shot.png");
    }
    std::filesystem::remove(path);
}

TEST_CASE("AttachmentRepository remove + cascade on entry delete", "[storage][attachment]")
{
    const auto path = freshDbPath("pm_test_attach_remove.db");
    {
        Database db = Database::open(path.string(), makeDek(0xB2));
        storage::migrate(db);
        EntryRepository entries(db);
        AttachmentRepository repo(db);

        const auto entryId = addEntry(db);
        const auto a1 = repo.add(samplePng(entryId));
        const auto a2 = repo.add(samplePng(entryId));

        REQUIRE(repo.remove(a1));
        REQUIRE_FALSE(repo.getById(a1).has_value());
        REQUIRE(repo.listForEntry(entryId).size() == 1);

        // Deleting the entry cascades to its remaining attachments.
        REQUIRE(entries.remove(entryId));
        REQUIRE_FALSE(repo.getById(a2).has_value());
        REQUIRE(repo.listForEntry(entryId).empty());
    }
    std::filesystem::remove(path);
}
