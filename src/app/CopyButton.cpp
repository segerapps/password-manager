#include "app/CopyButton.h"

#include <QTimer>

#include "app/Clipboard.h"

namespace pm::app {

CopyButton::CopyButton(QWidget *parent) : QPushButton("Copy", parent)
{
    setFixedWidth(72);
    connect(this, &QPushButton::clicked, this, &CopyButton::doCopy);
}

void CopyButton::setValueProvider(std::function<QString()> provider)
{
    provider_ = std::move(provider);
}

void CopyButton::doCopy()
{
    if (!provider_) {
        return;
    }
    const QString value = provider_();
    if (value.isEmpty()) {
        return;
    }
    secureCopy(value);

    setText("Copied ✓");
    setEnabled(false);
    QTimer::singleShot(1500, this, [this]() {
        setText("Copy");
        setEnabled(true);
    });
}

} // namespace pm::app
