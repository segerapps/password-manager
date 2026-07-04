#pragma once

#include <cstdint>

#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QVBoxLayout;

namespace pm::model {
struct Entry;
}
namespace pm::storage {
class Database;
}

namespace pm::app {

// The right-hand detail pane: shows a selected entry's fields with reveal/copy/
// open, an attachments (screenshots) thumbnail grid with add/view/delete, plus
// Edit and Delete. Needs the open DB (for attachments) via setDatabase().
class EntryDetailView : public QWidget
{
    Q_OBJECT

public:
    explicit EntryDetailView(QWidget *parent = nullptr);

    void setDatabase(pm::storage::Database *db); // non-owning
    void showEntry(const pm::model::Entry &entry);
    void showPlaceholder();

signals:
    void editRequested(qlonglong entryId);
    void deleteRequested(qlonglong entryId);

private slots:
    void addAttachment();
    void openAttachment(QListWidgetItem *item);

private:
    void clearFields();
    void loadAttachments(qlonglong entryId);

    qlonglong currentId_ = 0;
    pm::storage::Database *db_ = nullptr;

    QLabel *placeholder_ = nullptr;
    QWidget *content_ = nullptr;
    QLabel *title_ = nullptr;
    QLabel *typeChip_ = nullptr;
    QVBoxLayout *fieldsLayout_ = nullptr;
    QListWidget *attachments_ = nullptr;
    QPushButton *addAttachButton_ = nullptr;
    QLabel *footer_ = nullptr;
    QPushButton *editButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
};

} // namespace pm::app
