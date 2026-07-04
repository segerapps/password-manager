#include "core/storage/Database.h"

#include <string>

#include <sodium.h>
#include <sqlcipher/sqlite3.h>

#include "core/crypto/Crypto.h"

namespace pm::storage {

Database Database::open(const std::string &path, const crypto::SecureBuffer &dek,
                        bool createIfMissing)
{
    sqlite3 *db = nullptr;
    const int flags = SQLITE_OPEN_READWRITE | (createIfMissing ? SQLITE_OPEN_CREATE : 0);
    const int rc = sqlite3_open_v2(path.c_str(), &db, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = db ? sqlite3_errmsg(db) : "sqlite3_open_v2 failed";
        sqlite3_close(db); // safe on a partially-opened handle
        throw DbError(msg, rc);
    }

    Database database(db);
    database.applyKey(dek); // throws (and closes via dtor) if the key is wrong
    return database;
}

void Database::applyKey(const crypto::SecureBuffer &dek)
{
    if (dek.size() != crypto::kKeySize) {
        throw DbError("DEK must be 32 bytes", SQLITE_MISUSE);
    }

    // Raw-key form `PRAGMA key = "x'<64 hex>'"`: SQLCipher uses these 32 bytes
    // directly as the key and skips its own PBKDF2 (our DEK is already a proper
    // random 256-bit key derived through the wrapped-DEK hierarchy).
    char hex[2 * crypto::kKeySize + 1];
    sodium_bin2hex(hex, sizeof(hex), dek.data(), dek.size());

    // Reserve up front so the concatenations below never reallocate — a realloc
    // would free an intermediate buffer holding the hex key without zeroing it.
    std::string pragma;
    pragma.reserve(sizeof("PRAGMA key = \"x'';\";") + sizeof(hex));
    pragma += "PRAGMA key = \"x'";
    pragma += hex;
    pragma += "'\";";
    sodium_memzero(hex, sizeof(hex));

    try {
        exec(pragma);
    } catch (...) {
        sodium_memzero(pragma.data(), pragma.size());
        throw;
    }
    sodium_memzero(pragma.data(), pragma.size()); // minimise the key's lifetime

    // First real DB access forces SQLCipher to validate the key. A wrong key (or
    // a non-vault file) surfaces here as SQLITE_NOTADB rather than silently.
    exec("SELECT count(*) FROM sqlite_master;");

    // Per-connection: needed so deleting an entry cascades to its fields,
    // attachments, and tag links (see docs/DATA-MODEL.md §5).
    exec("PRAGMA foreign_keys = ON;");
}

void Database::exec(const std::string &sql)
{
    char *errmsg = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : sqlite3_errmsg(db_);
        sqlite3_free(errmsg);
        throw DbError(msg, rc);
    }
}

void Database::close() noexcept
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Database::~Database()
{
    close();
}

Database::Database(Database &&other) noexcept : db_(other.db_)
{
    other.db_ = nullptr;
}

Database &Database::operator=(Database &&other) noexcept
{
    if (this != &other) {
        close();
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

} // namespace pm::storage
