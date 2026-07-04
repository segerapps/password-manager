#include "core/storage/SettingsRepository.h"

#include <string>

#include "core/storage/Database.h"
#include "core/storage/detail/SqliteUtil.h"

namespace pm::storage {
using namespace detail;

std::optional<std::string> SettingsRepository::get(const std::string &key)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "SELECT value FROM settings WHERE key = ?;");
    bindText(s, 1, key);

    const int rc = sqlite3_step(s);
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        fail(h, rc);
    }
    if (sqlite3_column_type(s, 0) == SQLITE_NULL) {
        return std::nullopt;
    }
    return colText(s, 0);
}

int SettingsRepository::getInt(const std::string &key, int fallback)
{
    if (const auto value = get(key)) {
        try {
            return std::stoi(*value);
        } catch (...) {
            // fall through to fallback on a non-numeric value
        }
    }
    return fallback;
}

void SettingsRepository::set(const std::string &key, const std::string &value)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);");
    bindText(s, 1, key);
    bindText(s, 2, value);
    runDone(h, s);
}

} // namespace pm::storage
