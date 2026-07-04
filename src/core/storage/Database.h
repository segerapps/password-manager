#pragma once

#include <stdexcept>
#include <string>

#include "core/crypto/SecureBuffer.h"

// Opaque libsodium-style forward declaration so this header never needs to pull
// in <sqlcipher/sqlite3.h>. The SQLCipher dependency stays an implementation
// detail of Database.cpp (linked PRIVATE in CMake).
struct sqlite3;

namespace pm::storage {

// Thrown on any SQLite/SQLCipher failure. `code()` is the raw SQLite result code
// (e.g. SQLITE_NOTADB == wrong key / not a database).
class DbError : public std::runtime_error
{
public:
    DbError(const std::string &message, int code)
        : std::runtime_error(message), code_(code)
    {
    }

    int code() const noexcept { return code_; }

private:
    int code_;
};

// RAII handle to an open, keyed SQLCipher database.
//
// The database file is whole-DB encrypted by SQLCipher using the raw 32-byte DEK
// (see docs/DATA-MODEL.md §3.2) — we pass it as a raw key so SQLCipher does NOT
// run its own KDF over it (our DEK is already a proper random 256-bit key).
//
// Non-copyable (one owner of the handle); movable.
class Database
{
public:
    // Open the encrypted database at `path`, keyed with the raw DEK. With
    // `createIfMissing` (default) a missing file is created — used when creating a
    // vault; unlock passes `false` so a missing vault.db is an error, not a silent
    // empty vault. Throws DbError on failure / wrong key / non-vault file.
    // `path` is UTF-8.
    static Database open(const std::string &path, const crypto::SecureBuffer &dek,
                         bool createIfMissing = true);

    ~Database();

    Database(Database &&other) noexcept;
    Database &operator=(Database &&other) noexcept;

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Run one or more SQL statements that return no rows (DDL, PRAGMA, INSERT…).
    // Throws DbError on failure.
    void exec(const std::string &sql);

    // The underlying handle, for prepared-statement helpers / repositories.
    sqlite3 *handle() const noexcept { return db_; }

private:
    explicit Database(sqlite3 *db) noexcept : db_(db) {}

    // Apply the raw key, verify it, and enable foreign keys. Throws on a bad key.
    void applyKey(const crypto::SecureBuffer &dek);
    void close() noexcept;

    sqlite3 *db_ = nullptr;
};

} // namespace pm::storage
