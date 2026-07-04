#pragma once

#include <optional>

#include <QMainWindow>
#include <QString>

#include "core/vault/VaultSession.h"

class QStackedWidget;
class QTimer;

namespace pm::app {

class UnlockView;
class VaultPage;
class GeneratorPage;
class SettingsPage;
class NavRail;

// Top-level window. Outer stack: UnlockView ↔ the unlocked "shell" (NavRail +
// inner stack of Vault / Generator / Settings). Owns the live VaultSession.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override; // resets idle timer

private slots:
    void handleCreate(const QString &password);
    void handleUnlock(const QString &password);
    void handleLock();
    void handleChangePassword();
    void handleExportBackup();
    void handleRestoreBackup();
    void handleAutoLockChanged(int seconds);
    void onIdleTimeout();

private:
    QString vaultDir() const;
    void showUnlock();
    void enterVault();
    void startIdleTimer();

    QStackedWidget *stack_ = nullptr; // unlock ↔ shell
    UnlockView *unlock_ = nullptr;

    QWidget *shell_ = nullptr;
    NavRail *navRail_ = nullptr;
    QStackedWidget *inner_ = nullptr; // vault / generator / settings
    VaultPage *vault_ = nullptr;
    GeneratorPage *generator_ = nullptr;
    SettingsPage *settings_ = nullptr;

    QTimer *idleTimer_ = nullptr;
    int autoLockSeconds_ = 180;

    std::optional<pm::vault::VaultSession> session_;
};

} // namespace pm::app
