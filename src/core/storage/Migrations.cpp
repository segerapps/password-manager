#include "core/storage/Migrations.h"

#include <array>
#include <ctime>
#include <string>
#include <string_view>

#include <sqlcipher/sqlite3.h>

#include "core/storage/Database.h"

namespace pm::storage {
namespace {

struct Migration
{
    int version;
    std::string_view sql;
};

// Schema v1 — see docs/DATA-MODEL.md §5. The `schema_migrations` bookkeeping
// table is owned by the migration runner (created separately, below), so it is
// deliberately NOT part of this script. `PRAGMA foreign_keys` is set per
// connection in Database::open, so it is omitted here too (it is a no-op inside
// a transaction anyway).
constexpr std::string_view kSchemaV1 = R"SQL(
CREATE TABLE folders (
    id          INTEGER PRIMARY KEY,
    name        TEXT    NOT NULL,
    parent_id   INTEGER REFERENCES folders(id) ON DELETE SET NULL,
    created_at  INTEGER NOT NULL
);

CREATE TABLE entries (
    id          INTEGER PRIMARY KEY,
    type        TEXT    NOT NULL CHECK (type IN
                    ('LOGIN','API_KEY','SECURE_NOTE','WEB_ADDRESS')),
    title       TEXT    NOT NULL,
    folder_id   INTEGER REFERENCES folders(id) ON DELETE SET NULL,
    favorite    INTEGER NOT NULL DEFAULT 0,
    notes       TEXT,
    created_at  INTEGER NOT NULL,
    updated_at  INTEGER NOT NULL
);

CREATE TABLE entry_fields (
    id          INTEGER PRIMARY KEY,
    entry_id    INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    key         TEXT    NOT NULL,
    label       TEXT    NOT NULL,
    value       TEXT,
    type        TEXT    NOT NULL CHECK (type IN
                    ('TEXT','SECRET','URL','EMAIL','PHONE','TOTP',
                     'DATE','NUMBER','MULTILINE')),
    is_secret   INTEGER NOT NULL DEFAULT 0,
    position    INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE attachments (
    id          INTEGER PRIMARY KEY,
    entry_id    INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    filename    TEXT    NOT NULL,
    mime_type   TEXT    NOT NULL,
    byte_size   INTEGER NOT NULL,
    data        BLOB    NOT NULL,
    created_at  INTEGER NOT NULL
);

CREATE TABLE tags (
    id          INTEGER PRIMARY KEY,
    name        TEXT    NOT NULL UNIQUE
);

CREATE TABLE entry_tags (
    entry_id    INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    tag_id      INTEGER NOT NULL REFERENCES tags(id)    ON DELETE CASCADE,
    PRIMARY KEY (entry_id, tag_id)
);

CREATE TABLE settings (
    key         TEXT PRIMARY KEY,
    value       TEXT
);

CREATE INDEX idx_entries_type   ON entries(type);
CREATE INDEX idx_entries_folder ON entries(folder_id);
CREATE INDEX idx_entries_title  ON entries(title);
CREATE INDEX idx_fields_entry   ON entry_fields(entry_id);
CREATE INDEX idx_attach_entry   ON attachments(entry_id);
CREATE INDEX idx_entrytags_tag  ON entry_tags(tag_id);
)SQL";

// Schema v2 — adds the SSH_KEY entry type. SQLite cannot ALTER a CHECK
// constraint, so `entries` is rebuilt via the documented 12-step procedure:
// create the new table under a temp name, copy rows, drop the old table,
// rename, recreate the indexes (DROP TABLE took the old ones with it). The
// runner executes this with foreign_keys=OFF — with FKs ON, dropping `entries`
// would cascade-delete every row in entry_fields/attachments/entry_tags — and
// verifies with foreign_key_check before committing.
constexpr std::string_view kSchemaV2 = R"SQL(
CREATE TABLE entries_new (
    id          INTEGER PRIMARY KEY,
    type        TEXT    NOT NULL CHECK (type IN
                    ('LOGIN','API_KEY','SECURE_NOTE','WEB_ADDRESS','SSH_KEY')),
    title       TEXT    NOT NULL,
    folder_id   INTEGER REFERENCES folders(id) ON DELETE SET NULL,
    favorite    INTEGER NOT NULL DEFAULT 0,
    notes       TEXT,
    created_at  INTEGER NOT NULL,
    updated_at  INTEGER NOT NULL
);

INSERT INTO entries_new (id, type, title, folder_id, favorite, notes,
                         created_at, updated_at)
    SELECT id, type, title, folder_id, favorite, notes, created_at, updated_at
    FROM entries;

DROP TABLE entries;

ALTER TABLE entries_new RENAME TO entries;

CREATE INDEX idx_entries_type   ON entries(type);
CREATE INDEX idx_entries_folder ON entries(folder_id);
CREATE INDEX idx_entries_title  ON entries(title);
)SQL";

constexpr std::array<Migration, 2> kMigrations{{
    {1, kSchemaV1},
    {2, kSchemaV2},
}};

// True if PRAGMA foreign_key_check reports any violation. Run inside the
// migration transaction (with foreign_keys=OFF) so a table rebuild that broke
// referential integrity is caught before COMMIT.
bool hasForeignKeyViolations(Database &db)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db.handle(), "PRAGMA foreign_key_check;", -1, &stmt, nullptr)
        != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(sqlite3_errmsg(db.handle()), SQLITE_ERROR);
    }
    const bool violations = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return violations;
}

} // namespace

int latestSchemaVersion() noexcept
{
    int latest = 0;
    for (const auto &m : kMigrations) {
        if (m.version > latest) {
            latest = m.version;
        }
    }
    return latest;
}

int schemaVersion(Database &db)
{
    // `schema_migrations` may not exist yet on a brand-new database; a failed
    // prepare (no such table) simply means nothing has been applied.
    sqlite3_stmt *stmt = nullptr;
    const char *sql = "SELECT COALESCE(MAX(version), 0) FROM schema_migrations;";
    if (sqlite3_prepare_v2(db.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

int migrate(Database &db, int targetVersion)
{
    db.exec(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "    version    INTEGER PRIMARY KEY,"
        "    applied_at INTEGER NOT NULL"
        ");");

    int current = schemaVersion(db);
    const auto now = static_cast<long long>(std::time(nullptr));
    const int target = (targetVersion == kLatestVersion) ? latestSchemaVersion()
                                                         : targetVersion;

    // Table-rebuild migrations (e.g. v2) must run with foreign key enforcement
    // off: the PRAGMA is a no-op inside a transaction, so it is toggled here,
    // outside BEGIN, and restored afterwards. Integrity is re-checked per
    // migration via foreign_key_check before COMMIT.
    db.exec("PRAGMA foreign_keys = OFF;");
    try {
        for (const auto &m : kMigrations) {
            if (m.version <= current || m.version > target) {
                continue;
            }

            db.exec("BEGIN;");
            try {
                db.exec(std::string(m.sql));
                if (hasForeignKeyViolations(db)) {
                    throw DbError("migration v" + std::to_string(m.version)
                                      + " broke referential integrity",
                                  SQLITE_CONSTRAINT);
                }
                db.exec("INSERT INTO schema_migrations (version, applied_at) VALUES ("
                        + std::to_string(m.version) + ", " + std::to_string(now) + ");");
                db.exec("COMMIT;");
            } catch (...) {
                db.exec("ROLLBACK;");
                throw;
            }
            current = m.version;
        }
    } catch (...) {
        db.exec("PRAGMA foreign_keys = ON;");
        throw;
    }
    db.exec("PRAGMA foreign_keys = ON;");

    return current;
}

} // namespace pm::storage
