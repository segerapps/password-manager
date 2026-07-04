#include "core/storage/TagRepository.h"

#include "core/storage/Database.h"
#include "core/storage/detail/SqliteUtil.h"

namespace pm::storage {
using namespace detail;

std::optional<model::Tag> TagRepository::getByName(const std::string &name)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "SELECT id, name FROM tags WHERE name = ?;");
    bindText(s, 1, name);

    const int rc = sqlite3_step(s);
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        fail(h, rc);
    }
    model::Tag t;
    t.id = sqlite3_column_int64(s, 0);
    t.name = colText(s, 1);
    return t;
}

std::int64_t TagRepository::getOrCreate(const std::string &name)
{
    if (const auto existing = getByName(name)) {
        return existing->id;
    }
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "INSERT INTO tags (name) VALUES (?);");
    bindText(s, 1, name);
    runDone(h, s);
    return sqlite3_last_insert_rowid(h);
}

std::vector<model::Tag> TagRepository::list()
{
    sqlite3 *h = db_.handle();
    std::vector<model::Tag> tags;
    Stmt s;
    prepare(h, s, "SELECT id, name FROM tags ORDER BY name, id;");
    int rc = 0;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        model::Tag t;
        t.id = sqlite3_column_int64(s, 0);
        t.name = colText(s, 1);
        tags.push_back(std::move(t));
    }
    if (rc != SQLITE_DONE) {
        fail(h, rc);
    }
    return tags;
}

bool TagRepository::remove(std::int64_t id)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "DELETE FROM tags WHERE id = ?;"); // entry_tags links cascade
    sqlite3_bind_int64(s, 1, id);
    runDone(h, s);
    return sqlite3_changes(h) > 0;
}

void TagRepository::assign(std::int64_t entryId, std::int64_t tagId)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s,
            "INSERT OR IGNORE INTO entry_tags (entry_id, tag_id) VALUES (?, ?);");
    sqlite3_bind_int64(s, 1, entryId);
    sqlite3_bind_int64(s, 2, tagId);
    runDone(h, s);
}

void TagRepository::unassign(std::int64_t entryId, std::int64_t tagId)
{
    sqlite3 *h = db_.handle();
    Stmt s;
    prepare(h, s, "DELETE FROM entry_tags WHERE entry_id = ? AND tag_id = ?;");
    sqlite3_bind_int64(s, 1, entryId);
    sqlite3_bind_int64(s, 2, tagId);
    runDone(h, s);
}

std::vector<model::Tag> TagRepository::tagsForEntry(std::int64_t entryId)
{
    sqlite3 *h = db_.handle();
    std::vector<model::Tag> tags;
    Stmt s;
    prepare(h, s,
            "SELECT t.id, t.name FROM tags t "
            "JOIN entry_tags et ON et.tag_id = t.id "
            "WHERE et.entry_id = ? ORDER BY t.name, t.id;");
    sqlite3_bind_int64(s, 1, entryId);
    int rc = 0;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        model::Tag t;
        t.id = sqlite3_column_int64(s, 0);
        t.name = colText(s, 1);
        tags.push_back(std::move(t));
    }
    if (rc != SQLITE_DONE) {
        fail(h, rc);
    }
    return tags;
}

} // namespace pm::storage
