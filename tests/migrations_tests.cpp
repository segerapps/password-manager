#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

#include <sqlcipher/sqlite3.h>

#include "core/crypto/Crypto.h"
#include "core/crypto/SecureBuffer.h"
#include "core/storage/Database.h"
#include "core/storage/Migrations.h"

using namespace pm;
using pm::storage::Database;

namespace {

const struct SodiumInit
{
    SodiumInit() { crypto::init(); }
} g_sodium_migrations;

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

// Run a query returning a single integer (e.g. COUNT(*)).
long long scalar(Database &db, const std::string &sql)
{
    sqlite3_stmt *stmt = nullptr;
    REQUIRE(sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
    const long long value = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

} // namespace

TEST_CASE("migrate builds the v1 schema", "[storage][migrations]")
{
    const auto path = freshDbPath("pm_test_migrate.db");
    {
        Database db = Database::open(path.string(), makeDek(0x70));

        const int version = storage::migrate(db);
        REQUIRE(version == storage::latestSchemaVersion());
        REQUIRE(storage::schemaVersion(db) == version);

        for (const char *table : {"folders", "entries", "entry_fields", "attachments",
                                  "tags", "entry_tags", "settings", "schema_migrations"}) {
            const std::string sql =
                std::string("SELECT count(*) FROM sqlite_master "
                            "WHERE type='table' AND name='")
                + table + "';";
            INFO("missing table: " << table);
            REQUIRE(scalar(db, sql) == 1);
        }
    }
    std::filesystem::remove(path);
}

TEST_CASE("migrate is idempotent", "[storage][migrations]")
{
    const auto path = freshDbPath("pm_test_migrate_idem.db");
    {
        Database db = Database::open(path.string(), makeDek(0x71));

        REQUIRE(storage::migrate(db) == storage::latestSchemaVersion());
        REQUIRE(storage::migrate(db) == storage::latestSchemaVersion()); // second run = no-op

        // One row recorded per applied migration (versions 1..N are contiguous).
        REQUIRE(scalar(db, "SELECT count(*) FROM schema_migrations;")
                == storage::latestSchemaVersion());
    }
    std::filesystem::remove(path);
}

TEST_CASE("v1 vault upgrades to v2 with data intact", "[storage][migrations][upgrade]")
{
    const auto path = freshDbPath("pm_test_upgrade_v2.db");
    {
        Database db = Database::open(path.string(), makeDek(0x73));

        REQUIRE(storage::migrate(db, 1) == 1);

        // Fill a v1 vault the way the app would: a folder, a favorited + tagged
        // login with a secret field, and an attachment.
        db.exec("INSERT INTO folders (id, name, created_at) VALUES (1, 'Work', 0);");
        db.exec("INSERT INTO entries (id, type, title, folder_id, favorite, notes, "
                "created_at, updated_at) "
                "VALUES (1, 'LOGIN', 'GitHub', 1, 1, 'work account', 10, 20);");
        db.exec("INSERT INTO entry_fields (entry_id, key, label, value, type, "
                "is_secret, position) "
                "VALUES (1, 'password', 'Password', 'hunter2', 'SECRET', 1, 0);");
        db.exec("INSERT INTO attachments (entry_id, filename, mime_type, byte_size, "
                "data, created_at) "
                "VALUES (1, 'shot.png', 'image/png', 3, x'010203', 0);");
        db.exec("INSERT INTO tags (id, name) VALUES (1, 'dev');");
        db.exec("INSERT INTO entry_tags (entry_id, tag_id) VALUES (1, 1);");

        // The v1 CHECK constraint must not know SSH_KEY yet.
        REQUIRE_THROWS_AS(
            db.exec("INSERT INTO entries (type, title, created_at, updated_at) "
                    "VALUES ('SSH_KEY', 'vps', 0, 0);"),
            storage::DbError);

        REQUIRE(storage::migrate(db) == 2);
        REQUIRE(storage::schemaVersion(db) == 2);

        // Every pre-existing row survived the entries table rebuild.
        REQUIRE(scalar(db, "SELECT count(*) FROM entries;") == 1);
        REQUIRE(scalar(db, "SELECT count(*) FROM entry_fields;") == 1);
        REQUIRE(scalar(db, "SELECT count(*) FROM attachments;") == 1);
        REQUIRE(scalar(db, "SELECT count(*) FROM entry_tags;") == 1);
        REQUIRE(scalar(db,
                       "SELECT count(*) FROM entries WHERE id = 1 AND title = 'GitHub' "
                       "AND folder_id = 1 AND favorite = 1 AND notes = 'work account' "
                       "AND created_at = 10 AND updated_at = 20;")
                == 1);
        REQUIRE(scalar(db, "SELECT count(*) FROM entry_fields "
                           "WHERE entry_id = 1 AND value = 'hunter2';")
                == 1);

        // DROP TABLE took the old indexes with it; the migration recreates them.
        REQUIRE(scalar(db, "SELECT count(*) FROM sqlite_master WHERE type = 'index' "
                           "AND name IN ('idx_entries_type','idx_entries_folder',"
                           "'idx_entries_title');")
                == 3);

        // SSH_KEY is now a valid type; garbage still is not.
        db.exec("INSERT INTO entries (id, type, title, created_at, updated_at) "
                "VALUES (2, 'SSH_KEY', 'prod VPS', 0, 0);");
        REQUIRE_THROWS_AS(
            db.exec("INSERT INTO entries (type, title, created_at, updated_at) "
                    "VALUES ('BOGUS', 'x', 0, 0);"),
            storage::DbError);

        // FK enforcement is restored after the rebuild and cascades still fire
        // against the NEW entries table.
        REQUIRE(scalar(db, "PRAGMA foreign_keys;") == 1);
        db.exec("DELETE FROM entries WHERE id = 1;");
        REQUIRE(scalar(db, "SELECT count(*) FROM entry_fields;") == 0);
        REQUIRE(scalar(db, "SELECT count(*) FROM attachments;") == 0);
        REQUIRE(scalar(db, "SELECT count(*) FROM entry_tags;") == 0);
    }
    std::filesystem::remove(path);
}

TEST_CASE("deleting an entry cascades to its fields", "[storage][migrations][fk]")
{
    const auto path = freshDbPath("pm_test_fk.db");
    {
        Database db = Database::open(path.string(), makeDek(0x72));
        storage::migrate(db);

        db.exec("INSERT INTO entries (id, type, title, created_at, updated_at) "
                "VALUES (1, 'LOGIN', 'GitHub', 0, 0);");
        db.exec("INSERT INTO entry_fields (entry_id, key, label, value, type) "
                "VALUES (1, 'password', 'Password', 'secret', 'SECRET');");
        REQUIRE(scalar(db, "SELECT count(*) FROM entry_fields;") == 1);

        // ON DELETE CASCADE (with foreign_keys=ON, set in Database::open) must
        // remove the child field when its entry is deleted.
        db.exec("DELETE FROM entries WHERE id = 1;");
        REQUIRE(scalar(db, "SELECT count(*) FROM entry_fields;") == 0);
    }
    std::filesystem::remove(path);
}
