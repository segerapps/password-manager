#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/model/Attachment.h"

namespace pm::storage {

class Database;

// CRUD access to attachments (screenshots/files stored as BLOBs). Deleting the
// owning entry deletes its attachments (ON DELETE CASCADE).
class AttachmentRepository
{
public:
    explicit AttachmentRepository(Database &db) noexcept : db_(db) {}

    // Insert an attachment. `byteSize` is taken from the data length, so callers
    // need not set it. Returns the new attachment id.
    std::int64_t add(const model::Attachment &attachment);

    // Load one attachment. With `withData = false`, only metadata is read and
    // `data` is left empty (cheap for large blobs).
    std::optional<model::Attachment> getById(std::int64_t id, bool withData = true);

    // Metadata for every attachment of an entry (no blob bytes), newest first.
    std::vector<model::Attachment> listForEntry(std::int64_t entryId);

    bool remove(std::int64_t id);

private:
    Database &db_;
};

} // namespace pm::storage
