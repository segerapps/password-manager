#include "core/storage/AttachmentRepository.h"

#include "core/storage/Database.h"
#include "core/storage/detail/SqliteUtil.h"

namespace pm::storage {
using namespace detail;
namespace {

// Read metadata columns 0..5 (id, entry_id, filename, mime_type, byte_size,
// created_at). `data` is filled separately when requested.
model::Attachment readMeta(sqlite3_stmt *s)
{
    model::Attachment a;
    a.id = sqlite3_column_int64(s, 0);
    a.entryId = sqlite3_column_int64(s, 1);
    a.filename = colText(s, 2);
    a.mimeType = colText(s, 3);
    a.byteSize = sqlite3_column_int64(s, 4);
    a.createdAt = sqlite3_column_int64(s, 5);
    return a;
}

} // namespace

std::int64_t AttachmentRepository::add(const model::Attachment &attachment)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s,
            "INSERT INTO attachments "
            "(entry_id, filename, mime_type, byte_size, data, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?);");
    sqlite3_bind_int64(s, 1, attachment.entryId);
    bindText(s, 2, attachment.filename);
    bindText(s, 3, attachment.mimeType);
    sqlite3_bind_int64(s, 4, static_cast<std::int64_t>(attachment.data.size()));
    bindBlob(s, 5, attachment.data);
    sqlite3_bind_int64(s, 6, attachment.createdAt);
    runDone(h, s);
    return sqlite3_last_insert_rowid(h);
}

std::optional<model::Attachment> AttachmentRepository::getById(std::int64_t id, bool withData)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s,
            "SELECT id, entry_id, filename, mime_type, byte_size, created_at, data "
            "FROM attachments WHERE id = ?;");
    sqlite3_bind_int64(s, 1, id);

    const int rc = sqlite3_step(s);
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        fail(h, rc);
    }
    model::Attachment a = readMeta(s);
    if (withData) {
        a.data = colBlob(s, 6);
    }
    return a;
}

std::vector<model::Attachment> AttachmentRepository::listForEntry(std::int64_t entryId)
{
    sqlite3 *h = db_.handle();
    std::vector<model::Attachment> attachments;
    Stmt s;
    prepare(h, s,
            "SELECT id, entry_id, filename, mime_type, byte_size, created_at "
            "FROM attachments WHERE entry_id = ? ORDER BY created_at DESC, id DESC;");
    sqlite3_bind_int64(s, 1, entryId);
    int rc = 0;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        attachments.push_back(readMeta(s)); // metadata only; data stays empty
    }
    if (rc != SQLITE_DONE) {
        fail(h, rc);
    }
    return attachments;
}

bool AttachmentRepository::remove(std::int64_t id)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "DELETE FROM attachments WHERE id = ?;");
    sqlite3_bind_int64(s, 1, id);
    runDone(h, s);
    return sqlite3_changes(h) > 0;
}

} // namespace pm::storage
