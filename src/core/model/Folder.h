#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace pm::model {

// A grouping folder (a row in `folders`). `parentId` nests folders; a null parent
// is a top-level folder. Deleting a parent sets its children's parentId to null
// (ON DELETE SET NULL).
struct Folder
{
    std::int64_t id = 0; // 0 until persisted
    std::string name;
    std::optional<std::int64_t> parentId;
    std::int64_t createdAt = 0; // Unix epoch seconds
};

} // namespace pm::model
