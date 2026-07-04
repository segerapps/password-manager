#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

#include "core/crypto/Crypto.h"
#include "core/crypto/SecureBuffer.h"
#include "core/model/Entry.h"
#include "core/storage/Database.h"
#include "core/storage/EntryRepository.h"
#include "core/storage/Migrations.h"

using namespace pm;
using pm::model::Entry;
using pm::model::EntryType;
using pm::model::Field;
using pm::model::FieldType;
using pm::storage::Database;
using pm::storage::EntryRepository;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_repo;

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

Entry sampleLogin()
{
    Entry e;
    e.type = EntryType::Login;
    e.title = "GitHub";
    e.favorite = true;
    e.notes = "work account";
    e.createdAt = 1000;
    e.updatedAt = 1000;
    e.fields = {
        Field{0, "username", "Username", "aditya@example.com", FieldType::Email, false, 0},
        Field{0, "password", "Password", "s3cr3t!", FieldType::Secret, true, 1},
        Field{0, "url", "Address", "https://github.com", FieldType::Url, false, 2},
    };
    return e;
}

} // namespace

TEST_CASE("EntryRepository add + getById round-trips an entry", "[storage][repo]")
{
    const auto path = freshDbPath("pm_test_repo_rt.db");
    {
        Database db = Database::open(path.string(), makeDek(0x80));
        storage::migrate(db);
        EntryRepository repo(db);

        const std::int64_t id = repo.add(sampleLogin());
        REQUIRE(id > 0);

        const auto loaded = repo.getById(id);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->id == id);
        REQUIRE(loaded->type == EntryType::Login);
        REQUIRE(loaded->title == "GitHub");
        REQUIRE(loaded->favorite);
        REQUIRE(loaded->notes.has_value());
        REQUIRE(*loaded->notes == "work account");

        REQUIRE(loaded->fields.size() == 3);
        // Fields come back ordered by position.
        REQUIRE(loaded->fields[1].key == "password");
        REQUIRE(loaded->fields[1].value == "s3cr3t!");
        REQUIRE(loaded->fields[1].type == FieldType::Secret);
        REQUIRE(loaded->fields[1].isSecret);
    }
    std::filesystem::remove(path);
}

TEST_CASE("EntryRepository round-trips an SSH_KEY entry", "[storage][repo][ssh]")
{
    const auto path = freshDbPath("pm_test_repo_ssh.db");
    {
        Database db = Database::open(path.string(), makeDek(0x84));
        storage::migrate(db);
        EntryRepository repo(db);

        Entry e;
        e.type = EntryType::SshKey;
        e.title = "prod VPS";
        e.createdAt = 1000;
        e.updatedAt = 1000;
        e.fields = {
            Field{0, "host", "Host", "vps.example.com", FieldType::Text, false, 0},
            Field{0, "username", "Username", "deploy", FieldType::Text, false, 1},
            Field{0, "private_key", "Private key",
                  "-----BEGIN OPENSSH PRIVATE KEY-----\nabc\n-----END OPENSSH PRIVATE KEY-----",
                  FieldType::Multiline, true, 2},
        };

        const std::int64_t id = repo.add(e);
        REQUIRE(id > 0);

        const auto loaded = repo.getById(id);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->type == EntryType::SshKey);
        REQUIRE(loaded->fields.size() == 3);
        REQUIRE(loaded->fields[2].key == "private_key");
        REQUIRE(loaded->fields[2].type == FieldType::Multiline);
        REQUIRE(loaded->fields[2].isSecret);
        REQUIRE(loaded->fields[2].value.find("BEGIN OPENSSH") != std::string::npos);
    }
    std::filesystem::remove(path);
}

TEST_CASE("EntryRepository getById returns nullopt for a missing id", "[storage][repo]")
{
    const auto path = freshDbPath("pm_test_repo_missing.db");
    {
        Database db = Database::open(path.string(), makeDek(0x81));
        storage::migrate(db);
        EntryRepository repo(db);

        REQUIRE_FALSE(repo.getById(999).has_value());
    }
    std::filesystem::remove(path);
}

TEST_CASE("EntryRepository list returns all entries by title", "[storage][repo]")
{
    const auto path = freshDbPath("pm_test_repo_list.db");
    {
        Database db = Database::open(path.string(), makeDek(0x82));
        storage::migrate(db);
        EntryRepository repo(db);

        Entry b = sampleLogin();
        b.title = "Bravo";
        Entry a = sampleLogin();
        a.title = "Alpha";
        repo.add(b);
        repo.add(a);

        const auto all = repo.list();
        REQUIRE(all.size() == 2);
        REQUIRE(all[0].title == "Alpha"); // ordered by title
        REQUIRE(all[1].title == "Bravo");
        REQUIRE(all[0].fields.size() == 3);
    }
    std::filesystem::remove(path);
}

TEST_CASE("EntryRepository update replaces row and fields", "[storage][repo]")
{
    const auto path = freshDbPath("pm_test_repo_update.db");
    {
        Database db = Database::open(path.string(), makeDek(0x83));
        storage::migrate(db);
        EntryRepository repo(db);

        const std::int64_t id = repo.add(sampleLogin());

        Entry edit = *repo.getById(id);
        edit.title = "GitHub (work)";
        edit.favorite = false;
        edit.updatedAt = 2000;
        edit.fields.pop_back(); // drop the url field -> now 2 fields
        edit.fields[1].value = "rotated-secret";

        REQUIRE(repo.update(edit));

        const auto loaded = repo.getById(id);
        REQUIRE(loaded->title == "GitHub (work)");
        REQUIRE_FALSE(loaded->favorite);
        REQUIRE(loaded->updatedAt == 2000);
        REQUIRE(loaded->fields.size() == 2);
        REQUIRE(loaded->fields[1].value == "rotated-secret");
    }
    std::filesystem::remove(path);
}

TEST_CASE("EntryRepository update returns false for a missing id", "[storage][repo]")
{
    const auto path = freshDbPath("pm_test_repo_update_missing.db");
    {
        Database db = Database::open(path.string(), makeDek(0x84));
        storage::migrate(db);
        EntryRepository repo(db);

        Entry ghost = sampleLogin();
        ghost.id = 12345;
        REQUIRE_FALSE(repo.update(ghost));
    }
    std::filesystem::remove(path);
}

TEST_CASE("EntryRepository remove deletes the entry and its fields", "[storage][repo]")
{
    const auto path = freshDbPath("pm_test_repo_remove.db");
    {
        Database db = Database::open(path.string(), makeDek(0x85));
        storage::migrate(db);
        EntryRepository repo(db);

        const std::int64_t id = repo.add(sampleLogin());
        REQUIRE(repo.getById(id).has_value());

        REQUIRE(repo.remove(id));
        REQUIRE_FALSE(repo.getById(id).has_value()); // gone (fields cascaded)
        REQUIRE_FALSE(repo.remove(id));              // already gone
    }
    std::filesystem::remove(path);
}
