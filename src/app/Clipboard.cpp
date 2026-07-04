#include "app/Clipboard.h"

#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QMimeData>
#include <QTimer>

namespace pm::app {

namespace {
// The most recent secret we placed on the clipboard, so lock/quit can clear it
// only if it is still ours.
QString g_lastCopied;
} // namespace

void secureCopy(const QString &text, int ttlMs)
{
    auto *mime = new QMimeData;
    mime->setText(text);

    // Windows clipboard-format hints that keep secrets out of history / cloud.
    mime->setData("ExcludeClipboardContentFromMonitorProcessing", QByteArray());
    const QByteArray off(4, '\0'); // a DWORD 0
    mime->setData("CanIncludeInClipboardHistory", off);
    mime->setData("CanUploadToCloudClipboard", off);

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(mime); // takes ownership
    g_lastCopied = text;

    if (ttlMs > 0) {
        QTimer::singleShot(ttlMs, qApp, [text]() {
            QClipboard *cb = QApplication::clipboard();
            if (cb->text() == text) {
                cb->clear();
            }
            if (g_lastCopied == text) {
                g_lastCopied.clear();
            }
        });
    }
}

void clearSecureClipboard()
{
    if (g_lastCopied.isEmpty()) {
        return;
    }
    QClipboard *cb = QApplication::clipboard();
    if (cb->text() == g_lastCopied) {
        cb->clear();
    }
    g_lastCopied.clear();
}

} // namespace pm::app
