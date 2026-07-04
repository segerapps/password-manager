#pragma once

#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;

namespace pm::app {

// Settings page (docs/UI-SPEC.md §3.6): auto-lock timeout, change master password,
// lock, encrypted backup export/import, and vault info. Actions are emitted as
// signals; MainWindow (which owns the VaultSession) performs them.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setVaultPath(const QString &path);
    void setAutoLockSeconds(int seconds); // reflect the stored setting in the combo

signals:
    void changePasswordRequested();
    void lockRequested();
    void exportBackupRequested();
    void restoreBackupRequested();
    void autoLockChanged(int seconds);

private:
    QLabel *path_ = nullptr;
    QComboBox *autoLock_ = nullptr;
};

} // namespace pm::app
