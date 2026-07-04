#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/model/Entry.h"

namespace pm::storage {

class Database;

// CRUD access to entries and their fields. A thin data-access layer over the
// SQLCipher database — it persists exactly what the model carries (timestamps
// included) and contains no business logic or clock access; the vault/service
// layer (M3) owns those. All writes that touch multiple rows run in one
// transaction. Uses prepared statements with bound parameters throughout.
class EntryRepository
{
public:
    explicit EntryRepository(Database &db) noexcept : db_(db) {}

    // Insert the entry and its fields. Returns the new entry id.
    std::int64_t add(const model::Entry &entry);

    // Load an entry with its fields (ordered by position), or nullopt if absent.
    std::optional<model::Entry> getById(std::int64_t id);

    // Every entry (each with its fields), ordered by title.
    std::vector<model::Entry> list();

    // Replace the entry row and its fields. Returns false if `entry.id` is absent.
    bool update(const model::Entry &entry);

    // Delete the entry; its fields/attachments/tag links cascade. Returns false
    // if `id` did not exist.
    bool remove(std::int64_t id);

private:
    Database &db_;
};

} // namespace pm::storage
