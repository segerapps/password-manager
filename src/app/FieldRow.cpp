#include "app/FieldRow.h"

#include <QDesktopServices>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "app/CopyButton.h"

namespace pm::app {

FieldRow::FieldRow(const QString &label, const QString &value, bool secret, bool isUrl,
                   bool multiline, QWidget *parent)
    : QWidget(parent), value_(value), secret_(secret)
{
    auto *labelWidget = new QLabel(label.toUpper(), this);
    labelWidget->setObjectName("SectionLabel");

    QFont mono("JetBrains Mono");
    mono.setStyleHint(QFont::Monospace);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);
    outer->addWidget(labelWidget);

    auto *copy = new CopyButton(this);
    copy->setValueProvider([this]() { return value_; });

    if (multiline) {
        // Secret multiline (e.g. an SSH private key): the box holds a mask until
        // revealed — QPlainTextEdit has no Password echo mode.
        const QString mask = QStringLiteral("••••••••••••••••");
        auto *box = new QPlainTextEdit(secret_ ? mask : value, this);
        box->setReadOnly(true);
        box->setFont(mono);
        box->setFixedHeight(QFontMetrics(mono).lineSpacing() * 3 + 14); // ~3 lines
        outer->addWidget(box);

        auto *btnRow = new QHBoxLayout;
        btnRow->addStretch();
        if (secret_) {
            revealButton_ = new QPushButton("Show", this);
            revealButton_->setCheckable(true);
            revealButton_->setFixedWidth(64);
            connect(revealButton_, &QPushButton::toggled, this, [this, box, mask](bool on) {
                box->setPlainText(on ? value_ : mask);
                revealButton_->setText(on ? "Hide" : "Show");
            });
            btnRow->addWidget(revealButton_);
        }
        btnRow->addWidget(copy);
        outer->addLayout(btnRow);
        return;
    }

    field_ = new QLineEdit(value, this);
    field_->setReadOnly(true);
    field_->setFont(mono);
    if (secret_) {
        field_->setEchoMode(QLineEdit::Password);
    }

    auto *actions = new QHBoxLayout;
    actions->setSpacing(6);
    actions->addWidget(field_, 1);

    if (secret_) {
        revealButton_ = new QPushButton("Show", this);
        revealButton_->setCheckable(true);
        revealButton_->setFixedWidth(64);
        connect(revealButton_, &QPushButton::toggled, this, &FieldRow::toggleReveal);
        actions->addWidget(revealButton_);
    }

    actions->addWidget(copy);

    if (isUrl) {
        auto *open = new QPushButton("Open", this);
        open->setFixedWidth(64);
        const QString url = value_;
        connect(open, &QPushButton::clicked, this, [this, url]() {
            const QUrl parsed(url);
            const QString scheme = parsed.scheme().toLower();
            // Only open web links — never file://, UNC, or arbitrary app schemes
            // (a restored vault could carry a hostile URL).
            if (scheme == "http" || scheme == "https") {
                QDesktopServices::openUrl(parsed);
            } else {
                QMessageBox::warning(this, "Open URL",
                                     "Hanya tautan http/https yang bisa dibuka.");
            }
        });
        actions->addWidget(open);
    }

    outer->addLayout(actions);
}

void FieldRow::toggleReveal()
{
    const bool show = revealButton_->isChecked();
    field_->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    revealButton_->setText(show ? "Hide" : "Show");
}

} // namespace pm::app
