#include "app/VaultPage.h"

#include <exception>

#include <QButtonGroup>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include "app/EntryDetailView.h"
#include "app/EntryEditorDialog.h"
#include "core/storage/EntryRepository.h"

namespace pm::app {

namespace {

QPushButton *makeChip(const QString &text, int etype, QButtonGroup *group, QWidget *parent)
{
    auto *chip = new QPushButton(text, parent);
    chip->setObjectName("Chip");
    chip->setCheckable(true);
    chip->setProperty("etype", etype); // -1 = All, else EntryType index
    group->addButton(chip);
    return chip;
}

bool entryMatches(const model::Entry &e, const QString &query)
{
    if (QString::fromStdString(e.title).toLower().contains(query)) {
        return true;
    }
    if (e.notes && QString::fromStdString(*e.notes).toLower().contains(query)) {
        return true;
    }
    for (const auto &f : e.fields) {
        if (!f.isSecret && QString::fromStdString(f.value).toLower().contains(query)) {
            return true;
        }
    }
    return false;
}

} // namespace

VaultPage::VaultPage(QWidget *parent) : QWidget(parent)
{
    auto *header = new QLabel("Vault", this);
    header->setObjectName("Title");

    addButton_ = new QPushButton("+ Add Entry", this);
    addButton_->setObjectName("Primary");
    lockButton_ = new QPushButton("Lock", this);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(header);
    topRow->addStretch();
    topRow->addWidget(addButton_);
    topRow->addWidget(lockButton_);

    search_ = new QLineEdit(this);
    search_->setPlaceholderText("Search vault…");
    search_->setClearButtonEnabled(true);

    // Filter chips (exclusive). "All" selected by default.
    chips_ = new QButtonGroup(this);
    chips_->setExclusive(true);
    auto *chipRow = new QHBoxLayout;
    chipRow->setSpacing(6);
    auto *allChip = makeChip("All", -1, chips_, this);
    allChip->setChecked(true);
    chipRow->addWidget(allChip);
    chipRow->addWidget(makeChip("Logins", static_cast<int>(model::EntryType::Login), chips_, this));
    chipRow->addWidget(makeChip("API Keys", static_cast<int>(model::EntryType::ApiKey), chips_, this));
    chipRow->addWidget(makeChip("Notes", static_cast<int>(model::EntryType::SecureNote), chips_, this));
    chipRow->addWidget(makeChip("Addresses", static_cast<int>(model::EntryType::WebAddress), chips_, this));
    chipRow->addWidget(makeChip("SSH Keys", static_cast<int>(model::EntryType::SshKey), chips_, this));
    chipRow->addStretch();

    count_ = new QLabel(this);
    count_->setObjectName("Subtitle");

    list_ = new QListWidget(this);

    auto *left = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);
    leftLayout->addLayout(topRow);
    leftLayout->addWidget(search_);
    leftLayout->addLayout(chipRow);
    leftLayout->addWidget(count_);
    leftLayout->addWidget(list_, 1);

    detail_ = new EntryDetailView(this);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(left);
    splitter->addWidget(detail_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({380, 580});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->addWidget(splitter);

    connect(addButton_, &QPushButton::clicked, this, &VaultPage::addEntry);
    connect(lockButton_, &QPushButton::clicked, this, &VaultPage::lockRequested);
    connect(list_, &QListWidget::itemSelectionChanged, this, &VaultPage::onSelectionChanged);
    connect(detail_, &EntryDetailView::editRequested, this, &VaultPage::editEntry);
    connect(detail_, &EntryDetailView::deleteRequested, this, &VaultPage::deleteEntry);
    connect(search_, &QLineEdit::textChanged, this, &VaultPage::applyFilter);
    connect(chips_, &QButtonGroup::buttonClicked, this, &VaultPage::applyFilter);
}

void VaultPage::setDatabase(pm::storage::Database *db)
{
    db_ = db;
    detail_->setDatabase(db); // detail pane uses it for attachments
    if (db_) {
        refresh();
    } else {
        entries_.clear();
        list_->clear();
        count_->clear();
        detail_->showPlaceholder();
    }
}

void VaultPage::refresh()
{
    if (!db_) {
        return;
    }
    storage::EntryRepository repo(*db_);
    entries_ = repo.list();
    applyFilter();
}

void VaultPage::applyFilter()
{
    int selectedType = -1;
    if (auto *checked = chips_->checkedButton()) {
        selectedType = checked->property("etype").toInt();
    }
    const QString query = search_->text().trimmed().toLower();

    list_->clear();
    int shown = 0;
    for (const auto &e : entries_) {
        if (selectedType >= 0 && static_cast<int>(e.type) != selectedType) {
            continue;
        }
        if (!query.isEmpty() && !entryMatches(e, query)) {
            continue;
        }

        QString subtitle;
        for (const auto &f : e.fields) {
            if (f.key == "username" || f.key == "key_id" || f.key == "site_name") {
                subtitle = QString::fromStdString(f.value);
                break;
            }
        }
        QString text = QString::fromStdString(e.title);
        if (!subtitle.isEmpty()) {
            text += "\n" + subtitle;
        }
        auto *item = new QListWidgetItem(text, list_);
        item->setData(Qt::UserRole, static_cast<qlonglong>(e.id));
        ++shown;
    }

    if (entries_.empty()) {
        count_->setText("No entries yet — add your first one.");
    } else {
        count_->setText(QString("%1 of %2 shown").arg(shown).arg(entries_.size()));
    }
}

void VaultPage::onSelectionChanged()
{
    auto *item = list_->currentItem();
    if (!item || !db_) {
        detail_->showPlaceholder();
        return;
    }
    const qlonglong id = item->data(Qt::UserRole).toLongLong();
    storage::EntryRepository repo(*db_);
    if (const auto entry = repo.getById(id)) {
        detail_->showEntry(*entry);
    } else {
        detail_->showPlaceholder();
    }
}

void VaultPage::selectId(qlonglong id)
{
    for (int i = 0; i < list_->count(); ++i) {
        if (list_->item(i)->data(Qt::UserRole).toLongLong() == id) {
            list_->setCurrentRow(i);
            return;
        }
    }
}

void VaultPage::addEntry()
{
    if (!db_) {
        return;
    }
    EntryEditorDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    model::Entry entry = dialog.result();
    const auto now = QDateTime::currentSecsSinceEpoch();
    entry.createdAt = now;
    entry.updatedAt = now;

    qlonglong newId = 0;
    try {
        storage::EntryRepository repo(*db_);
        newId = static_cast<qlonglong>(repo.add(entry));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Add Entry",
                              QString("Could not save the entry:\n%1").arg(e.what()));
        return;
    }
    refresh();
    selectId(newId);
}

void VaultPage::editEntry(qlonglong id)
{
    if (!db_) {
        return;
    }
    storage::EntryRepository repo(*db_);
    const auto existing = repo.getById(id);
    if (!existing) {
        return;
    }

    EntryEditorDialog dialog(this);
    dialog.setEntry(*existing);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    model::Entry entry = dialog.result();
    entry.updatedAt = QDateTime::currentSecsSinceEpoch();
    try {
        repo.update(entry);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Edit Entry",
                              QString("Could not save the entry:\n%1").arg(e.what()));
        return;
    }
    refresh();
    selectId(id);
}

void VaultPage::deleteEntry(qlonglong id)
{
    if (!db_) {
        return;
    }
    const auto answer = QMessageBox::question(
        this, "Delete entry", "Delete this entry permanently? This cannot be undone.");
    if (answer != QMessageBox::Yes) {
        return;
    }
    try {
        storage::EntryRepository repo(*db_);
        repo.remove(id);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Delete entry",
                              QString("Could not delete the entry:\n%1").arg(e.what()));
        return;
    }
    refresh();
    detail_->showPlaceholder();
}

} // namespace pm::app
