#include "app/GeneratorPage.h"

#include <QCheckBox>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <sodium.h>

#include "app/CopyButton.h"

namespace pm::app {

GeneratorPage::GeneratorPage(QWidget *parent) : QWidget(parent)
{
    auto *title = new QLabel("Password Generator", this);
    title->setObjectName("Title");

    output_ = new QLineEdit(this);
    output_->setReadOnly(true);
    QFont mono("JetBrains Mono");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(18);
    output_->setFont(mono);
    output_->setAlignment(Qt::AlignCenter);

    auto *copy = new CopyButton(this);
    copy->setValueProvider([this]() { return output_->text(); });

    auto *regen = new QPushButton("Regenerate", this);
    regen->setObjectName("Primary");

    length_ = new QSlider(Qt::Horizontal, this);
    length_->setRange(8, 64);
    length_->setValue(20);
    lengthLabel_ = new QLabel(this);
    lengthLabel_->setFixedWidth(28);

    upper_ = new QCheckBox("Uppercase  A-Z", this);
    upper_->setChecked(true);
    lower_ = new QCheckBox("Lowercase  a-z", this);
    lower_->setChecked(true);
    digits_ = new QCheckBox("Digits  0-9", this);
    digits_->setChecked(true);
    symbols_ = new QCheckBox("Symbols  !@#$…", this);
    symbols_->setChecked(true);
    ambiguous_ = new QCheckBox("Exclude ambiguous (i l 1 L o 0 O)", this);

    auto *outRow = new QHBoxLayout;
    outRow->addWidget(output_, 1);
    outRow->addWidget(copy);

    auto *lenRow = new QHBoxLayout;
    lenRow->addWidget(new QLabel("Length", this));
    lenRow->addWidget(length_, 1);
    lenRow->addWidget(lengthLabel_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    layout->addWidget(title);
    layout->addSpacing(6);
    layout->addLayout(outRow);
    layout->addWidget(regen);
    layout->addSpacing(8);
    layout->addLayout(lenRow);
    layout->addWidget(upper_);
    layout->addWidget(lower_);
    layout->addWidget(digits_);
    layout->addWidget(symbols_);
    layout->addWidget(ambiguous_);
    layout->addStretch();

    connect(regen, &QPushButton::clicked, this, &GeneratorPage::generate);
    connect(length_, &QSlider::valueChanged, this, &GeneratorPage::generate);
    for (QCheckBox *cb : {upper_, lower_, digits_, symbols_, ambiguous_}) {
        connect(cb, &QCheckBox::toggled, this, &GeneratorPage::generate);
    }
    generate();
}

void GeneratorPage::generate()
{
    QString charset;
    if (upper_->isChecked()) {
        charset += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }
    if (lower_->isChecked()) {
        charset += "abcdefghijklmnopqrstuvwxyz";
    }
    if (digits_->isChecked()) {
        charset += "0123456789";
    }
    if (symbols_->isChecked()) {
        charset += "!@#$%^&*()-_=+[]{};:,.?";
    }
    if (ambiguous_->isChecked()) {
        const QString amb = "il1Lo0O";
        QString filtered;
        for (const QChar c : charset) {
            if (!amb.contains(c)) {
                filtered += c;
            }
        }
        charset = filtered;
    }

    lengthLabel_->setText(QString::number(length_->value()));

    if (charset.isEmpty()) {
        output_->setText("(select at least one character set)");
        return;
    }

    const int n = length_->value();
    QString out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const std::uint32_t idx = randombytes_uniform(static_cast<std::uint32_t>(charset.size()));
        out += charset.at(static_cast<int>(idx));
    }
    output_->setText(out);
}

} // namespace pm::app
