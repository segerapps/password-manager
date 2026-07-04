#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/model/Folder.h"

namespace pm::storage {

class Database;

// CRUD access to folders. Deleting a folder sets its child folders' parentId and
// any entries' folderId to null (ON DELETE SET NULL) — entries are not deleted.
class FolderRepository
{
public:
    explicit FolderRepository(Database &db) noexcept : db_(db) {}

    std::int64_t add(const model::Folder &folder);
    std::optional<model::Folder> getById(std::int64_t id);
    std::vector<model::Folder> list(); // ordered by name
    bool update(const model::Folder &folder);
    bool remove(std::int64_t id);

private:
    Database &db_;
};

} // namespace pm::storage
