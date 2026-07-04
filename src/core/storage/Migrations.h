#pragma once

namespace pm::storage {

class Database;

// The newest schema version this build knows how to produce.
int latestSchemaVersion() noexcept;

// The schema version currently recorded in the database (0 on a brand-new DB
// where no migration has run yet).
int schemaVersion(Database &db);

// Apply every migration newer than the database's current version, in order,
// each wrapped in its own transaction, recording each in `schema_migrations`.
// Returns the resulting version. Idempotent — a no-op on an up-to-date
// database. Throws DbError and rolls back the offending migration on failure.
//
// `targetVersion` stops after that version instead of going all the way to
// latest — used by tests to build an old-schema database and exercise the
// upgrade path. Leave it defaulted in production code.
inline constexpr int kLatestVersion = -1;
int migrate(Database &db, int targetVersion = kLatestVersion);

} // namespace pm::storage
