#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Plain domain types mirroring docs/DATA-MODEL.md. No Qt, no SQL — these are the
// objects the storage repositories read/write and the UI later binds to.
namespace pm::model {

enum class EntryType
{
    Login,
    ApiKey,
    SecureNote,
    WebAddress,
    SshKey, // append-only: UI maps combo indexes to these values
};

enum class FieldType
{
    Text,
    Secret,
    Url,
    Email,
    Phone,
    Totp,
    Date,
    Number,
    Multiline,
};

// The database stores these enums as TEXT (DATA-MODEL §4). These convert to/from
// the canonical strings ('LOGIN', 'SECRET', …). `*FromString` returns nullopt on
// an unrecognised value.
std::string toString(EntryType type);
std::string toString(FieldType type);
std::optional<EntryType> entryTypeFromString(std::string_view text);
std::optional<FieldType> fieldTypeFromString(std::string_view text);

// One field of an entry (a row in entry_fields).
struct Field
{
    std::int64_t id = 0; // 0 until persisted
    std::string key;     // canonical key, e.g. "password"
    std::string label;   // display label, e.g. "Password"
    std::string value;
    FieldType type = FieldType::Text;
    bool isSecret = false;
    int position = 0;
};

// One stored item (a row in entries) plus its fields.
struct Entry
{
    std::int64_t id = 0; // 0 until persisted
    EntryType type = EntryType::Login;
    std::string title;
    std::optional<std::int64_t> folderId;
    bool favorite = false;
    std::optional<std::string> notes;
    std::int64_t createdAt = 0; // Unix epoch seconds — set by the caller/service
    std::int64_t updatedAt = 0;
    std::vector<Field> fields;
};

} // namespace pm::model
