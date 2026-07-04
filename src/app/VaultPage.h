#pragma once

#include <vector>

#include <QWidget>

#include "core/model/Entry.h"

class QButtonGroup;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace pm::storage {
class Database;
}

namespace pm::app {

class EntryDetailView;

// The vault screen: master-detail with search + type filter chips. Entry list on
// the left, detail pane on the right (docs/UI-SPEC.md §3.2).
class VaultPage : public QWidget
{
    Q_OBJECT

public:
    explicit VaultPage(QWidget *parent = nullptr);

    void setDatabase(pm::storage::Database *db);
    void refresh();

signals:
    void lockRequested();

private slots:
    void addEntry();
    void onSelectionChanged();
    void editEntry(qlonglong id);
    void deleteEntry(qlonglong id);
    void applyFilter();

private:
    void selectId(qlonglong id);

    pm::storage::Database *db_ = nullptr;
    std::vector<model::Entry> entries_; // full set; the list shows a filtered view

    QLineEdit *search_ = nullptr;
    QButtonGroup *chips_ = nullptr;
    QLabel *count_ = nullptr;
    QListWidget *list_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *lockButton_ = nullptr;
    EntryDetailView *detail_ = nullptr;
};

} // namespace pm::app
