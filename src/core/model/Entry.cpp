#include "core/model/Entry.h"

namespace pm::model {

std::string toString(EntryType type)
{
    switch (type) {
    case EntryType::Login:
        return "LOGIN";
    case EntryType::ApiKey:
        return "API_KEY";
    case EntryType::SecureNote:
        return "SECURE_NOTE";
    case EntryType::WebAddress:
        return "WEB_ADDRESS";
    case EntryType::SshKey:
        return "SSH_KEY";
    }
    return "LOGIN";
}

std::string toString(FieldType type)
{
    switch (type) {
    case FieldType::Text:
        return "TEXT";
    case FieldType::Secret:
        return "SECRET";
    case FieldType::Url:
        return "URL";
    case FieldType::Email:
        return "EMAIL";
    case FieldType::Phone:
        return "PHONE";
    case FieldType::Totp:
        return "TOTP";
    case FieldType::Date:
        return "DATE";
    case FieldType::Number:
        return "NUMBER";
    case FieldType::Multiline:
        return "MULTILINE";
    }
    return "TEXT";
}

std::optional<EntryType> entryTypeFromString(std::string_view text)
{
    if (text == "LOGIN")
        return EntryType::Login;
    if (text == "API_KEY")
        return EntryType::ApiKey;
    if (text == "SECURE_NOTE")
        return EntryType::SecureNote;
    if (text == "WEB_ADDRESS")
        return EntryType::WebAddress;
    if (text == "SSH_KEY")
        return EntryType::SshKey;
    return std::nullopt;
}

std::optional<FieldType> fieldTypeFromString(std::string_view text)
{
    if (text == "TEXT")
        return FieldType::Text;
    if (text == "SECRET")
        return FieldType::Secret;
    if (text == "URL")
        return FieldType::Url;
    if (text == "EMAIL")
        return FieldType::Email;
    if (text == "PHONE")
        return FieldType::Phone;
    if (text == "TOTP")
        return FieldType::Totp;
    if (text == "DATE")
        return FieldType::Date;
    if (text == "NUMBER")
        return FieldType::Number;
    if (text == "MULTILINE")
        return FieldType::Multiline;
    return std::nullopt;
}

} // namespace pm::model
