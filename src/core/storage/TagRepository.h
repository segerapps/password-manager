#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/model/Tag.h"

namespace pm::storage {

class Database;

// CRUD access to tags plus the many-to-many links between entries and tags
// (the `entry_tags` table). Deleting a tag or an entry removes its links
// (ON DELETE CASCADE).
class TagRepository
{
public:
    explicit TagRepository(Database &db) noexcept : db_(db) {}

    // Return the id of the tag with this name, creating it if absent. Tag names
    // are unique, so this is the safe way to "add".
    std::int64_t getOrCreate(const std::string &name);

    std::optional<model::Tag> getByName(const std::string &name);
    std::vector<model::Tag> list(); // ordered by name
    bool remove(std::int64_t id);

    // Many-to-many links.
    void assign(std::int64_t entryId, std::int64_t tagId);   // idempotent
    void unassign(std::int64_t entryId, std::int64_t tagId); // no-op if absent
    std::vector<model::Tag> tagsForEntry(std::int64_t entryId);

private:
    Database &db_;
};

} // namespace pm::storage
