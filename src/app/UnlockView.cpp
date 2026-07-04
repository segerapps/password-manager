#include "app/UnlockView.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace pm::app {

UnlockView::UnlockView(QWidget *parent) : QWidget(parent)
{
    // A centered, fixed-width card.
    auto *card = new QFrame(this);
    card->setObjectName("Card");
    card->setFixedWidth(380);

    title_ = new QLabel("Digital Fortitude", card);
    title_->setObjectName("Title");
    title_->setAlignment(Qt::AlignCenter);

    subtitle_ = new QLabel("Secure Vault Entry", card);
    subtitle_->setObjectName("Subtitle");
    subtitle_->setAlignment(Qt::AlignCenter);

    auto *pwLabel = new QLabel("MASTER PASSWORD", card);
    pwLabel->setObjectName("SectionLabel");

    password_ = new QLineEdit(card);
    password_->setEchoMode(QLineEdit::Password);
    password_->setPlaceholderText("Enter master password");

    revealButton_ = new QPushButton("Show", card);
    revealButton_->setCheckable(true);
    revealButton_->setFixedWidth(64);

    auto *pwRow = new QHBoxLayout;
    pwRow->addWidget(password_);
    pwRow->addWidget(revealButton_);

    confirmLabel_ = new QLabel("CONFIRM PASSWORD", card);
    confirmLabel_->setObjectName("SectionLabel");
    confirm_ = new QLineEdit(card);
    confirm_->setEchoMode(QLineEdit::Password);
    confirm_->setPlaceholderText("Re-enter master password");

    error_ = new QLabel(card);
    error_->setObjectName("Error");
    error_->setWordWrap(true);
    error_->hide();

    submitButton_ = new QPushButton("Unlock Vault", card);
    submitButton_->setObjectName("Primary");
    submitButton_->setDefault(true);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(10);
    cardLayout->addWidget(title_);
    cardLayout->addWidget(subtitle_);
    cardLayout->addSpacing(12);
    cardLayout->addWidget(pwLabel);
    cardLayout->addLayout(pwRow);
    cardLayout->addWidget(confirmLabel_);
    cardLayout->addWidget(confirm_);
    cardLayout->addWidget(error_);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(submitButton_);

    // Center the card in the view.
    auto *outer = new QVBoxLayout(this);
    outer->addStretch();
    auto *centerRow = new QHBoxLayout;
    centerRow->addStretch();
    centerRow->addWidget(card);
    centerRow->addStretch();
    outer->addLayout(centerRow);
    outer->addStretch();

    connect(submitButton_, &QPushButton::clicked, this, &UnlockView::submit);
    connect(revealButton_, &QPushButton::toggled, this, &UnlockView::toggleReveal);
    connect(password_, &QLineEdit::returnPressed, this, &UnlockView::submit);
    connect(confirm_, &QLineEdit::returnPressed, this, &UnlockView::submit);

    setCreateMode(false);
}

void UnlockView::setCreateMode(bool create)
{
    createMode_ = create;
    subtitle_->setText(create ? "Create your vault" : "Secure Vault Entry");
    submitButton_->setText(create ? "Create Vault" : "Unlock Vault");
    confirmLabel_->setVisible(create);
    confirm_->setVisible(create);
    reset();
}

void UnlockView::reset()
{
    password_->clear();
    confirm_->clear();
    error_->hide();
    password_->setFocus();
}

void UnlockView::showError(const QString &message)
{
    error_->setText(message);
    error_->show();
}

void UnlockView::toggleReveal()
{
    const bool show = revealButton_->isChecked();
    const auto mode = show ? QLineEdit::Normal : QLineEdit::Password;
    password_->setEchoMode(mode);
    confirm_->setEchoMode(mode);
    revealButton_->setText(show ? "Hide" : "Show");
}

void UnlockView::submit()
{
    const QString pw = password_->text();

    if (createMode_) {
        if (pw.length() < kMinMasterPasswordLength) {
            showError(QString("Master password must be at least %1 characters.")
                          .arg(kMinMasterPasswordLength));
            return;
        }
        if (pw != confirm_->text()) {
            showError("The two passwords do not match.");
            return;
        }
        error_->hide();
        emit createRequested(pw);
        return;
    }

    if (pw.isEmpty()) {
        showError("Enter your master password.");
        return;
    }
    error_->hide();
    emit unlockRequested(pw);
}

} // namespace pm::app
