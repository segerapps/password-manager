#include "app/TotpRow.h"

#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include "app/CopyButton.h"
#include "app/Theme.h"
#include "core/crypto/Totp.h"

namespace pm::app {

TotpRow::TotpRow(const QString &label, const QString &base32Secret, QWidget *parent)
    : QWidget(parent), secret_(base32Secret)
{
    auto *labelWidget = new QLabel(label.toUpper(), this);
    labelWidget->setObjectName("SectionLabel");

    QFont mono("JetBrains Mono");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(16);

    code_ = new QLabel(this);
    code_->setFont(mono);
    code_->setStyleSheet(QString("color:%1; letter-spacing:2px;").arg(tokens::primary));

    countdown_ = new QLabel(this);
    countdown_->setObjectName("Subtitle");

    auto *copy = new CopyButton(this);
    copy->setValueProvider([this]() { return currentCode(); });

    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    row->addWidget(code_);
    row->addWidget(countdown_);
    row->addStretch();
    row->addWidget(copy);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);
    outer->addWidget(labelWidget);
    outer->addLayout(row);

    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &TotpRow::refresh);
    timer_->start();
    refresh();
}

QString TotpRow::currentCode() const
{
    const auto code = crypto::totpCode(secret_.toStdString(),
                                       QDateTime::currentSecsSinceEpoch());
    return code ? QString::fromStdString(*code) : QString();
}

void TotpRow::refresh()
{
    const QString code = currentCode();
    if (code.isEmpty()) {
        code_->setText("—");
        countdown_->setText("secret TOTP tidak valid");
        timer_->stop(); // a bad secret won't get better next second
        return;
    }

    // "123 456" — grouped like every authenticator app.
    code_->setText(code.left(3) + ' ' + code.mid(3));
    const int remaining =
        crypto::totpSecondsRemaining(QDateTime::currentSecsSinceEpoch());
    countdown_->setText(QString("berlaku %1 dtk").arg(remaining));
}

} // namespace pm::app
