#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace pm::app {

// Minimum master-password length, shared by the create/unlock flow and the
// change-master-password dialog.
inline constexpr int kMinMasterPasswordLength = 8;

// The first screen: either "create a new vault" (first run) or "unlock" an
// existing one. It performs NO crypto itself — it just collects the password and
// emits a request; MainWindow drives the VaultSession and reports back via
// showError(). See docs/UI-SPEC.md §3.1.
class UnlockView : public QWidget
{
    Q_OBJECT

public:
    explicit UnlockView(QWidget *parent = nullptr);

    // create = true → first run: ask to set + confirm a new master password.
    void setCreateMode(bool create);
    void showError(const QString &message);
    void reset(); // clear fields + error

signals:
    void unlockRequested(const QString &password);
    void createRequested(const QString &password);

private slots:
    void submit();
    void toggleReveal();

private:
    bool createMode_ = false;
    QLabel *title_ = nullptr;
    QLabel *subtitle_ = nullptr;
    QLineEdit *password_ = nullptr;
    QLabel *confirmLabel_ = nullptr;
    QLineEdit *confirm_ = nullptr;
    QPushButton *revealButton_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QLabel *error_ = nullptr;
};

} // namespace pm::app
