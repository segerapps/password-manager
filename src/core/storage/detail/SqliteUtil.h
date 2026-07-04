#pragma once

// Internal helpers shared by the storage repositories — bind/column helpers, an
// RAII prepared-statement, and error mapping. Only ever included by repository
// .cpp files inside pm_core (it pulls in <sqlcipher/sqlite3.h>), never by a
// public header.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sqlcipher/sqlite3.h>

#include "core/storage/Database.h" // DbError

namespace pm::storage::detail {

// RAII for a prepared statement so every path finalises it.
struct Stmt
{
    sqlite3_stmt *p = nullptr;
    ~Stmt() { sqlite3_finalize(p); }
    operator sqlite3_stmt *() const noexcept { return p; }
};

[[noreturn]] inline void fail(sqlite3 *h, int rc)
{
    throw DbError(sqlite3_errmsg(h), rc);
}

inline void prepare(sqlite3 *h, Stmt &s, const char *sql)
{
    const int rc = sqlite3_prepare_v2(h, sql, -1, &s.p, nullptr);
    if (rc != SQLITE_OK) {
        fail(h, rc);
    }
}

// Step a statement that should complete without returning rows.
inline void runDone(sqlite3 *h, sqlite3_stmt *s)
{
    const int rc = sqlite3_step(s);
    if (rc != SQLITE_DONE) {
        fail(h, rc);
    }
}

// Bindings — SQLITE_TRANSIENT makes SQLite copy, so temporaries are safe.
inline void bindText(sqlite3_stmt *s, int i, const std::string &v)
{
    sqlite3_bind_text(s, i, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
}

inline void bindOptText(sqlite3_stmt *s, int i, const std::optional<std::string> &v)
{
    if (v) {
        bindText(s, i, *v);
    } else {
        sqlite3_bind_null(s, i);
    }
}

inline void bindOptInt(sqlite3_stmt *s, int i, const std::optional<std::int64_t> &v)
{
    if (v) {
        sqlite3_bind_int64(s, i, *v);
    } else {
        sqlite3_bind_null(s, i);
    }
}

inline void bindBlob(sqlite3_stmt *s, int i, const std::vector<unsigned char> &v)
{
    if (v.empty()) {
        sqlite3_bind_zeroblob(s, i, 0); // a zero-length, non-NULL blob
    } else {
        sqlite3_bind_blob(s, i, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
    }
}

// Column readers.
inline std::string colText(sqlite3_stmt *s, int i)
{
    const auto *t = reinterpret_cast<const char *>(sqlite3_column_text(s, i));
    return t ? std::string(t) : std::string();
}

inline std::optional<std::string> colOptText(sqlite3_stmt *s, int i)
{
    if (sqlite3_column_type(s, i) == SQLITE_NULL) {
        return std::nullopt;
    }
    return colText(s, i);
}

inline std::optional<std::int64_t> colOptInt(sqlite3_stmt *s, int i)
{
    if (sqlite3_column_type(s, i) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_int64(s, i);
}

inline std::vector<unsigned char> colBlob(sqlite3_stmt *s, int i)
{
    const auto *p = static_cast<const unsigned char *>(sqlite3_column_blob(s, i));
    const int n = sqlite3_column_bytes(s, i);
    if (!p || n <= 0) {
        return {};
    }
    return std::vector<unsigned char>(p, p + n);
}

} // namespace pm::storage::detail
