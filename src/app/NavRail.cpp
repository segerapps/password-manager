#include "app/NavRail.h"

#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace pm::app {

NavRail::NavRail(QWidget *parent) : QWidget(parent)
{
    setObjectName("NavRail");
    setFixedWidth(184);

    auto *title = new QLabel("🔐 Vault", this);
    title->setObjectName("NavTitle");

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 18, 12, 18);
    layout->setSpacing(4);
    layout->addWidget(title);
    layout->addSpacing(16);

    const char *items[] = {"Vault", "Generator", "Settings"};
    for (int i = 0; i < 3; ++i) {
        auto *button = new QPushButton(items[i], this);
        button->setObjectName("NavItem");
        button->setCheckable(true);
        group_->addButton(button, i);
        layout->addWidget(button);
    }
    layout->addStretch();

    if (auto *first = group_->button(0)) {
        first->setChecked(true);
    }
    connect(group_, &QButtonGroup::idClicked, this, &NavRail::navSelected);
}

void NavRail::setCurrent(int index)
{
    if (auto *button = group_->button(index)) {
        button->setChecked(true);
    }
    emit navSelected(index);
}

} // namespace pm::app
