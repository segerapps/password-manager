#include "core/storage/FolderRepository.h"

#include "core/storage/Database.h"
#include "core/storage/detail/SqliteUtil.h"

namespace pm::storage {
using namespace detail;
namespace {

model::Folder readRow(sqlite3_stmt *s)
{
    model::Folder f;
    f.id = sqlite3_column_int64(s, 0);
    f.name = colText(s, 1);
    f.parentId = colOptInt(s, 2);
    f.createdAt = sqlite3_column_int64(s, 3);
    return f;
}

} // namespace

std::int64_t FolderRepository::add(const model::Folder &folder)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s,
            "INSERT INTO folders (name, parent_id, created_at) VALUES (?, ?, ?);");
    bindText(s, 1, folder.name);
    bindOptInt(s, 2, folder.parentId);
    sqlite3_bind_int64(s, 3, folder.createdAt);
    runDone(h, s);
    return sqlite3_last_insert_rowid(h);
}

std::optional<model::Folder> FolderRepository::getById(std::int64_t id)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s,
            "SELECT id, name, parent_id, created_at FROM folders WHERE id = ?;");
    sqlite3_bind_int64(s, 1, id);

    const int rc = sqlite3_step(s);
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        fail(h, rc);
    }
    return readRow(s);
}

std::vector<model::Folder> FolderRepository::list()
{
    sqlite3 *h = db_.handle();
    std::vector<model::Folder> folders;
    Stmt s;
    prepare(h, s, "SELECT id, name, parent_id, created_at FROM folders ORDER BY name, id;");
    int rc = 0;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        folders.push_back(readRow(s));
    }
    if (rc != SQLITE_DONE) {
        fail(h, rc);
    }
    return folders;
}

bool FolderRepository::update(const model::Folder &folder)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "UPDATE folders SET name = ?, parent_id = ? WHERE id = ?;");
    bindText(s, 1, folder.name);
    bindOptInt(s, 2, folder.parentId);
    sqlite3_bind_int64(s, 3, folder.id);
    runDone(h, s);
    return sqlite3_changes(h) > 0;
}

bool FolderRepository::remove(std::int64_t id)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "DELETE FROM folders WHERE id = ?;");
    sqlite3_bind_int64(s, 1, id);
    runDone(h, s);
    return sqlite3_changes(h) > 0;
}

} // namespace pm::storage
