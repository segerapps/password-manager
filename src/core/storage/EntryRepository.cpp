#include "core/storage/EntryRepository.h"

#include <string>
#include <utility>

#include "core/storage/Database.h"
#include "core/storage/detail/SqliteUtil.h"

namespace pm::storage {
using namespace detail;
namespace {

// entries columns: id,type,title,folder_id,favorite,notes,created_at,updated_at
constexpr const char *kEntryColumns =
    "id, type, title, folder_id, favorite, notes, created_at, updated_at";

model::Entry readEntryRow(sqlite3_stmt *s)
{
    model::Entry e;
    e.id = sqlite3_column_int64(s, 0);
    e.type = model::entryTypeFromString(colText(s, 1)).value_or(model::EntryType::Login);
    e.title = colText(s, 2);
    e.folderId = colOptInt(s, 3);
    e.favorite = sqlite3_column_int(s, 4) != 0;
    e.notes = colOptText(s, 5);
    e.createdAt = sqlite3_column_int64(s, 6);
    e.updatedAt = sqlite3_column_int64(s, 7);
    return e;
}

// Bind the 7 entry body columns to positions 1..7 (id is auto / in the WHERE).
void bindEntryBody(sqlite3_stmt *s, const model::Entry &e)
{
    bindText(s, 1, model::toString(e.type));
    bindText(s, 2, e.title);
    bindOptInt(s, 3, e.folderId);
    sqlite3_bind_int(s, 4, e.favorite ? 1 : 0);
    bindOptText(s, 5, e.notes);
    sqlite3_bind_int64(s, 6, e.createdAt);
    sqlite3_bind_int64(s, 7, e.updatedAt);
}

void insertField(sqlite3 *h, std::int64_t entryId, const model::Field &f)
{
    Stmt s;
    prepare(h, s,
            "INSERT INTO entry_fields "
            "(entry_id, key, label, value, type, is_secret, position) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);");
    sqlite3_bind_int64(s, 1, entryId);
    bindText(s, 2, f.key);
    bindText(s, 3, f.label);
    bindText(s, 4, f.value);
    bindText(s, 5, model::toString(f.type));
    sqlite3_bind_int(s, 6, f.isSecret ? 1 : 0);
    sqlite3_bind_int(s, 7, f.position);
    runDone(h, s);
}

std::vector<model::Field> loadFields(sqlite3 *h, std::int64_t entryId)
{
    std::vector<model::Field> fields;
    Stmt s;
    prepare(h, s,
            "SELECT id, key, label, value, type, is_secret, position "
            "FROM entry_fields WHERE entry_id = ? ORDER BY position, id;");
    sqlite3_bind_int64(s, 1, entryId);

    int rc = 0;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        model::Field f;
        f.id = sqlite3_column_int64(s, 0);
        f.key = colText(s, 1);
        f.label = colText(s, 2);
        f.value = colText(s, 3);
        f.type = model::fieldTypeFromString(colText(s, 4)).value_or(model::FieldType::Text);
        f.isSecret = sqlite3_column_int(s, 5) != 0;
        f.position = sqlite3_column_int(s, 6);
        fields.push_back(std::move(f));
    }
    if (rc != SQLITE_DONE) {
        fail(h, rc);
    }
    return fields;
}

} // namespace

std::int64_t EntryRepository::add(const model::Entry &entry)
{
    sqlite3 *h = db_.handle();
    db_.exec("BEGIN;");
    std::int64_t newId = 0;
    try {
        {
            Stmt s;
            prepare(h, s,
                    "INSERT INTO entries "
                    "(type, title, folder_id, favorite, notes, created_at, updated_at) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?);");
            bindEntryBody(s, entry);
            runDone(h, s);
        }
        newId = sqlite3_last_insert_rowid(h);

        for (const auto &f : entry.fields) {
            insertField(h, newId, f);
        }
        db_.exec("COMMIT;");
    } catch (...) {
        db_.exec("ROLLBACK;");
        throw;
    }
    return newId;
}

std::optional<model::Entry> EntryRepository::getById(std::int64_t id)
{
    sqlite3 *h = db_.handle();
    model::Entry entry;
    {
        Stmt s;
        prepare(h, s,
                (std::string("SELECT ") + kEntryColumns + " FROM entries WHERE id = ?;")
                    .c_str());
        sqlite3_bind_int64(s, 1, id);

        const int rc = sqlite3_step(s);
        if (rc == SQLITE_DONE) {
            return std::nullopt;
        }
        if (rc != SQLITE_ROW) {
            fail(h, rc);
        }
        entry = readEntryRow(s);
    }
    entry.fields = loadFields(h, id);
    return entry;
}

std::vector<model::Entry> EntryRepository::list()
{
    sqlite3 *h = db_.handle();
    std::vector<model::Entry> entries;
    {
        Stmt s;
        prepare(h, s,
                (std::string("SELECT ") + kEntryColumns + " FROM entries ORDER BY title, id;")
                    .c_str());
        int rc = 0;
        while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
            entries.push_back(readEntryRow(s));
        }
        if (rc != SQLITE_DONE) {
            fail(h, rc);
        }
    }
    // Load fields after the list statement is finalised.
    for (auto &e : entries) {
        e.fields = loadFields(h, e.id);
    }
    return entries;
}

bool EntryRepository::update(const model::Entry &entry)
{
    sqlite3 *h = db_.handle();
    db_.exec("BEGIN;");
    try {
        int changed = 0;
        {
            Stmt s;
            prepare(h, s,
                    "UPDATE entries SET "
                    "type = ?, title = ?, folder_id = ?, favorite = ?, "
                    "notes = ?, created_at = ?, updated_at = ? WHERE id = ?;");
            bindEntryBody(s, entry);
            sqlite3_bind_int64(s, 8, entry.id);
            runDone(h, s);
            changed = sqlite3_changes(h);
        }
        if (changed == 0) {
            db_.exec("ROLLBACK;");
            return false; // no such entry
        }

        // Replace the field set wholesale (simpler than diffing).
        {
            Stmt s;
            prepare(h, s, "DELETE FROM entry_fields WHERE entry_id = ?;");
            sqlite3_bind_int64(s, 1, entry.id);
            runDone(h, s);
        }
        for (const auto &f : entry.fields) {
            insertField(h, entry.id, f);
        }
        db_.exec("COMMIT;");
        return true;
    } catch (...) {
        db_.exec("ROLLBACK;");
        throw;
    }
}

bool EntryRepository::remove(std::int64_t id)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "DELETE FROM entries WHERE id = ?;"); // fields cascade
    sqlite3_bind_int64(s, 1, id);
    runDone(h, s);
    return sqlite3_changes(h) > 0;
}

} // namespace pm::storage
