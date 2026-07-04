#pragma once

#include <functional>

#include <QPushButton>

namespace pm::app {

// A copy button that pulls the current value from a provider (so it copies the
// live value even when a field is masked), copies it via secureCopy, and shows a
// brief "Copied ✓" confirmation.
class CopyButton : public QPushButton
{
    Q_OBJECT

public:
    explicit CopyButton(QWidget *parent = nullptr);

    void setValueProvider(std::function<QString()> provider);

private slots:
    void doCopy();

private:
    std::function<QString()> provider_;
};

} // namespace pm::app
