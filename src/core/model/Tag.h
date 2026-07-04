#pragma once

#include <cstdint>
#include <string>

namespace pm::model {

// A tag (a row in `tags`). `name` is unique. Entries link to tags many-to-many
// through the `entry_tags` table.
struct Tag
{
    std::int64_t id = 0; // 0 until persisted
    std::string name;
};

} // namespace pm::model
