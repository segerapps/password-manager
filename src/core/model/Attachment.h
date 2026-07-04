#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pm::model {

// A file/screenshot attached to an entry (a row in `attachments`). The bytes live
// in `data` as a BLOB and are covered by the whole-DB SQLCipher encryption.
// `data` may be empty when only metadata was loaded (e.g. listing attachments).
struct Attachment
{
    std::int64_t id = 0;      // 0 until persisted
    std::int64_t entryId = 0; // owning entry
    std::string filename;
    std::string mimeType; // e.g. "image/png"
    std::int64_t byteSize = 0;
    std::vector<unsigned char> data;
    std::int64_t createdAt = 0; // Unix epoch seconds
};

} // namespace pm::model
