#include "app/MainWindow.h"

#include <exception>
#include <string>

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "app/Clipboard.h"
#include "app/GeneratorPage.h"
#include "app/NavRail.h"
#include "app/SettingsPage.h"
#include "app/UnlockView.h"
#include "app/VaultPage.h"
#include "core/storage/SettingsRepository.h"
#include "core/vault/Backup.h"

namespace pm::app {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Password Manager");
    resize(1228, 680);

    stack_ = new QStackedWidget(this);
    unlock_ = new UnlockView(this);

    // The unlocked shell: nav rail on the left, a content stack on the right.
    shell_ = new QWidget(this);
    navRail_ = new NavRail(shell_);
    vault_ = new VaultPage(shell_);
    generator_ = new GeneratorPage(shell_);
    settings_ = new SettingsPage(shell_);

    inner_ = new QStackedWidget(shell_);
    inner_->addWidget(vault_);     // 0
    inner_->addWidget(generator_); // 1
    inner_->addWidget(settings_);  // 2

    // Top bar with a hamburger that fully shows/hides the nav rail.
    auto *topBar = new QWidget(shell_);
    topBar->setObjectName("TopBar");
    auto *hamburger = new QPushButton("☰", topBar); // ☰
    hamburger->setObjectName("Hamburger");
    hamburger->setFixedSize(42, 30);
    hamburger->setToolTip("Show / hide navigation");
    auto *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(8, 5, 8, 5);
    topBarLayout->addWidget(hamburger);
    topBarLayout->addStretch();
    connect(hamburger, &QPushButton::clicked, this,
            [this]() { navRail_->setVisible(!navRail_->isVisible()); });

    // Body: nav rail beside the content stack.
    auto *body = new QWidget(shell_);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(navRail_);
    bodyLayout->addWidget(inner_, 1);

    auto *shellLayout = new QVBoxLayout(shell_);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(topBar);
    shellLayout->addWidget(body, 1);

    stack_->addWidget(unlock_);
    stack_->addWidget(shell_);
    setCentralWidget(stack_);

    connect(unlock_, &UnlockView::createRequested, this, &MainWindow::handleCreate);
    connect(unlock_, &UnlockView::unlockRequested, this, &MainWindow::handleUnlock);
    connect(vault_, &VaultPage::lockRequested, this, &MainWindow::handleLock);
    connect(settings_, &SettingsPage::lockRequested, this, &MainWindow::handleLock);
    connect(settings_, &SettingsPage::changePasswordRequested, this,
            &MainWindow::handleChangePassword);
    connect(settings_, &SettingsPage::exportBackupRequested, this,
            &MainWindow::handleExportBackup);
    connect(settings_, &SettingsPage::restoreBackupRequested, this,
            &MainWindow::handleRestoreBackup);
    connect(settings_, &SettingsPage::autoLockChanged, this,
            &MainWindow::handleAutoLockChanged);
    connect(navRail_, &NavRail::navSelected, inner_, &QStackedWidget::setCurrentIndex);

    // Idle auto-lock: a global event filter resets this timer on any user input.
    idleTimer_ = new QTimer(this);
    idleTimer_->setSingleShot(true);
    connect(idleTimer_, &QTimer::timeout, this, &MainWindow::onIdleTimeout);
    qApp->installEventFilter(this);

    // Clear any copied secret from the clipboard when the app quits.
    connect(qApp, &QCoreApplication::aboutToQuit, this, []() { clearSecureClipboard(); });

    settings_->setVaultPath(QDir::toNativeSeparators(vaultDir()));
    unlock_->setCreateMode(!pm::vault::VaultSession::exists(vaultDir().toStdString()));
    showUnlock();
}

QString MainWindow::vaultDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

void MainWindow::showUnlock()
{
    stack_->setCurrentWidget(unlock_);
    unlock_->reset();
}

void MainWindow::enterVault()
{
    vault_->setDatabase(&session_->database());

    // Load the per-vault auto-lock setting (default 180s) and arm the idle timer.
    autoLockSeconds_ =
        storage::SettingsRepository(session_->database()).getInt("auto_lock_seconds", 180);
    settings_->setAutoLockSeconds(autoLockSeconds_);
    startIdleTimer();

    navRail_->setCurrent(0);
    inner_->setCurrentIndex(0);
    stack_->setCurrentWidget(shell_);
}

void MainWindow::handleCreate(const QString &password)
{
    const std::string dir = vaultDir().toStdString();
    try {
        session_ = pm::vault::VaultSession::create(dir, password.toStdString());
    } catch (const std::exception &e) {
        unlock_->showError(QString("Could not create the vault:\n%1").arg(e.what()));
        return;
    }
    enterVault();
}

void MainWindow::handleUnlock(const QString &password)
{
    const std::string dir = vaultDir().toStdString();
    try {
        session_ = pm::vault::VaultSession::unlock(dir, password.toStdString());
    } catch (const pm::vault::WrongPassword &) {
        unlock_->showError("Wrong master password.");
        return;
    } catch (const std::exception &e) {
        unlock_->showError(QString("Could not open the vault:\n%1").arg(e.what()));
        return;
    }
    enterVault();
}

void MainWindow::handleLock()
{
    idleTimer_->stop();
    clearSecureClipboard(); // don't leave a copied secret pasteable after lock
    if (session_) {
        session_->lock();
    }
    session_.reset();
    vault_->setDatabase(nullptr);
    unlock_->setCreateMode(false); // the vault exists now → unlock mode
    showUnlock();
}

void MainWindow::handleChangePassword()
{
    if (!session_) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Change Master Password");
    dialog.setMinimumWidth(380);

    auto *pw = new QLineEdit(&dialog);
    pw->setEchoMode(QLineEdit::Password);
    auto *confirm = new QLineEdit(&dialog);
    confirm->setEchoMode(QLineEdit::Password);

    auto *form = new QFormLayout;
    form->addRow("New password", pw);
    form->addRow("Confirm", confirm);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    while (true) {
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        if (pw->text().length() < kMinMasterPasswordLength) {
            QMessageBox::warning(this, "Change Master Password",
                                 QString("Master password minimal %1 karakter.")
                                     .arg(kMinMasterPasswordLength));
            continue;
        }
        if (pw->text() != confirm->text()) {
            QMessageBox::warning(this, "Change Master Password",
                                 "Konfirmasi tidak cocok.");
            continue;
        }
        break;
    }

    try {
        session_->changeMasterPassword(pw->text().toStdString());
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Change Master Password",
                              QString("Gagal mengganti password:\n%1").arg(e.what()));
        return;
    }
    QMessageBox::information(this, "Change Master Password",
                             "Master password berhasil diganti.");
}

void MainWindow::handleExportBackup()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Export Encrypted Backup", "vault-backup.pmbackup",
        "Password Manager backup (*.pmbackup)");
    if (path.isEmpty()) {
        return;
    }
    try {
        pm::vault::exportBackup(vaultDir().toStdString(), path.toStdString());
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Export Backup",
                              QString("Gagal mengekspor backup:\n%1").arg(e.what()));
        return;
    }
    QMessageBox::information(
        this, "Export Backup",
        "Backup terenkripsi tersimpan:\n" + QDir::toNativeSeparators(path)
            + "\n\nButuh master password yang sama untuk membukanya di PC mana pun.");
}

void MainWindow::handleRestoreBackup()
{
    const auto confirm = QMessageBox::warning(
        this, "Restore from Backup",
        "Restore akan MENGGANTI vault saat ini dengan isi backup. Vault akan "
        "terkunci; buka dengan master password milik backup.\n\nLanjutkan?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, "Restore from Backup", QString(), "Password Manager backup (*.pmbackup)");
    if (path.isEmpty()) {
        return;
    }

    // Close the open database so its files can be overwritten, then restore.
    if (session_) {
        session_->lock();
    }
    session_.reset();
    vault_->setDatabase(nullptr);

    try {
        pm::vault::importBackup(path.toStdString(), vaultDir().toStdString(), /*overwrite=*/true);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Restore from Backup",
                              QString("Gagal me-restore backup:\n%1").arg(e.what()));
        unlock_->setCreateMode(!pm::vault::VaultSession::exists(vaultDir().toStdString()));
        showUnlock();
        return;
    }

    unlock_->setCreateMode(false);
    showUnlock();
    QMessageBox::information(
        this, "Restore from Backup",
        "Backup berhasil di-restore. Buka dengan master password milik backup.");
}

void MainWindow::startIdleTimer()
{
    if (session_ && autoLockSeconds_ > 0) {
        idleTimer_->start(autoLockSeconds_ * 1000);
    } else {
        idleTimer_->stop(); // locked, or "Never"
    }
}

void MainWindow::onIdleTimeout()
{
    if (!session_) {
        return;
    }
    // Don't lock while a modal dialog is open (editing / viewing) — defer instead.
    if (QApplication::activeModalWidget() != nullptr) {
        startIdleTimer();
        return;
    }
    handleLock();
}

void MainWindow::handleAutoLockChanged(int seconds)
{
    autoLockSeconds_ = seconds;
    if (session_) {
        storage::SettingsRepository(session_->database())
            .set("auto_lock_seconds", std::to_string(seconds));
    }
    startIdleTimer();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::KeyPress:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        if (idleTimer_->isActive()) {
            idleTimer_->start(); // user is active → restart the idle countdown
        }
        break;
    default:
        break;
    }
    return QMainWindow::eventFilter(watched, event);
}

} // namespace pm::app
