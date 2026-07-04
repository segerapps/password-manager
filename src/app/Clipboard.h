#pragma once

#include <QString>

namespace pm::app {

// Copy text to the clipboard with Windows privacy protections: excluded from
// clipboard monitors, Win+V history, and cloud clipboard (PLAN §3.5). After
// `ttlMs` the clipboard is auto-cleared — but only if it still holds this value
// (so we never wipe something the user copied afterwards).
void secureCopy(const QString &text, int ttlMs = 20000);

// Clear the clipboard now if it still holds the value of the most recent
// secureCopy (call this on vault lock and on app quit).
void clearSecureClipboard();

} // namespace pm::app
