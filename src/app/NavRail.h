#pragma once

#include <QWidget>

class QButtonGroup;

namespace pm::app {

// Left navigation rail: Vault / Generator / Settings (docs/UI-SPEC.md §2). Emits
// the selected index. Showing/hiding the whole rail is driven by the top-bar
// hamburger in MainWindow.
class NavRail : public QWidget
{
    Q_OBJECT

public:
    explicit NavRail(QWidget *parent = nullptr);

    void setCurrent(int index);

signals:
    void navSelected(int index);

private:
    QButtonGroup *group_ = nullptr;
};

} // namespace pm::app
