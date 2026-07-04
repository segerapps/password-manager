#pragma once

#include <optional>
#include <string>

namespace pm::storage {

class Database;

// Key/value access to the vault's `settings` table (auto-lock timeout, clipboard
// TTL, theme, …). Stored inside the encrypted DB, so settings travel with the
// vault and its backups.
class SettingsRepository
{
public:
    explicit SettingsRepository(Database &db) noexcept : db_(db) {}

    std::optional<std::string> get(const std::string &key);
    int getInt(const std::string &key, int fallback);
    void set(const std::string &key, const std::string &value);

private:
    Database &db_;
};

} // namespace pm::storage
