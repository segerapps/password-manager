#include "app/SettingsPage.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace pm::app {

SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent)
{
    auto *title = new QLabel("Settings", this);
    title->setObjectName("Title");

    auto *securityLabel = new QLabel("SECURITY", this);
    securityLabel->setObjectName("SectionLabel");

    autoLock_ = new QComboBox(this);
    autoLock_->addItem("1 minute", 60);
    autoLock_->addItem("3 minutes", 180);
    autoLock_->addItem("5 minutes", 300);
    autoLock_->addItem("15 minutes", 900);
    autoLock_->addItem("30 minutes", 1800);
    autoLock_->addItem("Never", 0);
    autoLock_->setCurrentIndex(1); // 3 minutes default
    auto *autoLockRow = new QHBoxLayout;
    autoLockRow->addWidget(new QLabel("Auto-lock after idle", this));
    autoLockRow->addStretch();
    autoLockRow->addWidget(autoLock_);

    auto *changePw = new QPushButton("Change Master Password…", this);
    auto *lockButton = new QPushButton("Lock Vault", this);

    auto *vaultLabel = new QLabel("VAULT", this);
    vaultLabel->setObjectName("SectionLabel");
    auto *exportButton = new QPushButton("Export Encrypted Backup…", this);
    auto *restoreButton = new QPushButton("Restore from Backup…", this);
    path_ = new QLabel(this);
    path_->setObjectName("Subtitle");
    path_->setWordWrap(true);

    auto *note = new QLabel(
        "Tidak ada pemulihan master password (zero-knowledge). Lupa password = data "
        "hilang — simpan backup terenkripsi secara berkala (tombol di atas).",
        this);
    note->setObjectName("Subtitle");
    note->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(securityLabel);
    layout->addLayout(autoLockRow);
    layout->addWidget(changePw);
    layout->addWidget(lockButton);
    layout->addSpacing(16);
    layout->addWidget(vaultLabel);
    layout->addWidget(exportButton);
    layout->addWidget(restoreButton);
    layout->addWidget(path_);
    layout->addWidget(note);
    layout->addStretch();

    connect(changePw, &QPushButton::clicked, this, &SettingsPage::changePasswordRequested);
    connect(lockButton, &QPushButton::clicked, this, &SettingsPage::lockRequested);
    connect(exportButton, &QPushButton::clicked, this, &SettingsPage::exportBackupRequested);
    connect(restoreButton, &QPushButton::clicked, this, &SettingsPage::restoreBackupRequested);
    connect(autoLock_, &QComboBox::currentIndexChanged, this,
            [this](int) { emit autoLockChanged(autoLock_->currentData().toInt()); });
}

void SettingsPage::setVaultPath(const QString &path)
{
    path_->setText("Lokasi vault: " + path);
}

void SettingsPage::setAutoLockSeconds(int seconds)
{
    const int index = autoLock_->findData(seconds);
    autoLock_->blockSignals(true);
    autoLock_->setCurrentIndex(index >= 0 ? index : 1); // fall back to 3 minutes
    autoLock_->blockSignals(false);
}

} // namespace pm::app
