#include "app/EntryDetailView.h"

#include <QByteArray>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "app/FieldRow.h"
#include "app/Theme.h"
#include "app/TotpRow.h"
#include "core/model/Attachment.h"
#include "core/model/Entry.h"
#include "core/storage/AttachmentRepository.h"
#include "core/storage/Database.h"

namespace pm::app {

namespace {
QString typeText(model::EntryType t)
{
    switch (t) {
    case model::EntryType::Login:
        return "LOGIN";
    case model::EntryType::ApiKey:
        return "API KEY";
    case model::EntryType::SecureNote:
        return "NOTE";
    case model::EntryType::WebAddress:
        return "ADDRESS";
    case model::EntryType::SshKey:
        return "SSH KEY";
    }
    return "LOGIN";
}
} // namespace

EntryDetailView::EntryDetailView(QWidget *parent) : QWidget(parent)
{
    placeholder_ = new QLabel("Select an entry to view its details.", this);
    placeholder_->setObjectName("Subtitle");
    placeholder_->setAlignment(Qt::AlignCenter);

    // ── content ──
    content_ = new QWidget(this);

    title_ = new QLabel(content_);
    title_->setObjectName("Title");
    title_->setWordWrap(true);

    typeChip_ = new QLabel(content_);
    typeChip_->setStyleSheet(QString("color:%1; font-family:'JetBrains Mono',Consolas,monospace; "
                                     "letter-spacing:1px;")
                                 .arg(tokens::primary));

    editButton_ = new QPushButton("Edit", content_);
    deleteButton_ = new QPushButton("Delete", content_);
    deleteButton_->setStyleSheet(QString("color:%1;").arg(tokens::critical));

    auto *headerRow = new QHBoxLayout;
    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(2);
    titleCol->addWidget(title_);
    titleCol->addWidget(typeChip_);
    headerRow->addLayout(titleCol, 1);
    headerRow->addWidget(editButton_);
    headerRow->addWidget(deleteButton_);

    auto *fieldsHost = new QWidget(content_);
    fieldsLayout_ = new QVBoxLayout(fieldsHost);
    fieldsLayout_->setContentsMargins(0, 0, 0, 0);
    fieldsLayout_->setSpacing(14);

    // ── attachments / screenshots ──
    auto *attachLabel = new QLabel("ATTACHMENTS", content_);
    attachLabel->setObjectName("SectionLabel");

    attachments_ = new QListWidget(content_);
    attachments_->setViewMode(QListView::IconMode);
    attachments_->setIconSize(QSize(84, 84));
    attachments_->setResizeMode(QListView::Adjust);
    attachments_->setMovement(QListView::Static);
    attachments_->setSpacing(8);
    attachments_->setWordWrap(true);
    attachments_->setFixedHeight(128);

    // "+ Add" sits at the bottom-right of the attachments box.
    addAttachButton_ = new QPushButton("+ Add", content_);
    addAttachButton_->setFixedWidth(80);
    auto *addAttachRow = new QHBoxLayout;
    addAttachRow->setContentsMargins(0, 0, 0, 0);
    addAttachRow->addStretch();
    addAttachRow->addWidget(addAttachButton_);

    footer_ = new QLabel(content_);
    footer_->setObjectName("Subtitle");

    auto *contentLayout = new QVBoxLayout(content_);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->addLayout(headerRow);
    contentLayout->addSpacing(8);
    contentLayout->addWidget(fieldsHost);
    contentLayout->addSpacing(8);
    contentLayout->addWidget(attachLabel);
    contentLayout->addWidget(attachments_);
    contentLayout->addLayout(addAttachRow);
    contentLayout->addStretch();
    contentLayout->addWidget(footer_);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->addWidget(placeholder_);
    outer->addWidget(content_);

    connect(editButton_, &QPushButton::clicked, this,
            [this]() { emit editRequested(currentId_); });
    connect(deleteButton_, &QPushButton::clicked, this,
            [this]() { emit deleteRequested(currentId_); });
    connect(addAttachButton_, &QPushButton::clicked, this, &EntryDetailView::addAttachment);
    connect(attachments_, &QListWidget::itemClicked, this, &EntryDetailView::openAttachment);

    showPlaceholder();
}

void EntryDetailView::setDatabase(pm::storage::Database *db)
{
    db_ = db;
}

void EntryDetailView::clearFields()
{
    QLayoutItem *item = nullptr;
    while ((item = fieldsLayout_->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
}

void EntryDetailView::showPlaceholder()
{
    currentId_ = 0;
    clearFields();
    attachments_->clear();
    content_->hide();
    placeholder_->show();
}

void EntryDetailView::showEntry(const pm::model::Entry &entry)
{
    currentId_ = static_cast<qlonglong>(entry.id);
    clearFields();

    title_->setText(QString::fromStdString(entry.title));
    typeChip_->setText(typeText(entry.type));

    for (const auto &f : entry.fields) {
        if (f.type == model::FieldType::Totp) {
            // Live 6-digit code + countdown; the secret itself stays hidden.
            fieldsLayout_->addWidget(new TotpRow(QString::fromStdString(f.label),
                                                 QString::fromStdString(f.value)));
            continue;
        }
        const bool isUrl = (f.type == model::FieldType::Url);
        const bool multiline = (f.type == model::FieldType::Multiline);
        auto *row = new FieldRow(QString::fromStdString(f.label),
                                 QString::fromStdString(f.value), f.isSecret, isUrl,
                                 multiline);
        fieldsLayout_->addWidget(row);
    }
    if (entry.notes && !entry.notes->empty()) {
        auto *notes = new FieldRow("Notes", QString::fromStdString(*entry.notes), false,
                                   false, /*multiline=*/true);
        fieldsLayout_->addWidget(notes);
    }

    loadAttachments(currentId_);

    footer_->setText("Argon2id KDF · XChaCha20-Poly1305 · Encrypted at rest");

    placeholder_->hide();
    content_->show();
}

void EntryDetailView::loadAttachments(qlonglong entryId)
{
    attachments_->clear();
    if (!db_) {
        return;
    }
    storage::AttachmentRepository repo(*db_);
    for (const auto &meta : repo.listForEntry(entryId)) {
        const auto full = repo.getById(meta.id, /*withData=*/true);
        QPixmap thumb;
        if (full && !full->data.empty()) {
            thumb.loadFromData(full->data.data(), static_cast<uint>(full->data.size()));
        }

        QListWidgetItem *item =
            thumb.isNull()
                ? new QListWidgetItem(QString::fromStdString(meta.filename), attachments_)
                : new QListWidgetItem(QIcon(thumb), QString::fromStdString(meta.filename),
                                      attachments_);
        item->setData(Qt::UserRole, static_cast<qlonglong>(meta.id));
        item->setSizeHint(QSize(100, 112));
        item->setToolTip(QString::fromStdString(meta.filename));
    }
}

void EntryDetailView::addAttachment()
{
    if (!db_ || currentId_ == 0) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, "Add attachment", QString(),
        "Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    if (QFileInfo(path).size() > 10 * 1024 * 1024) { // check size before loading it all
        QMessageBox::warning(this, "Attachment", "File terlalu besar (maksimal 10 MB).");
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Attachment", "Could not read the selected file.");
        return;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    model::Attachment attachment;
    attachment.entryId = currentId_;
    attachment.filename = QFileInfo(path).fileName().toStdString();
    attachment.mimeType = QMimeDatabase().mimeTypeForFile(path).name().toStdString();
    const auto *p = reinterpret_cast<const unsigned char *>(bytes.constData());
    attachment.data.assign(p, p + bytes.size());
    attachment.createdAt = QDateTime::currentSecsSinceEpoch();

    try {
        storage::AttachmentRepository repo(*db_);
        repo.add(attachment);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Attachment",
                              QString("Could not save the attachment:\n%1").arg(e.what()));
        return;
    }
    loadAttachments(currentId_);
}

void EntryDetailView::openAttachment(QListWidgetItem *item)
{
    if (!db_ || !item) {
        return;
    }
    const qlonglong id = item->data(Qt::UserRole).toLongLong();
    storage::AttachmentRepository repo(*db_);
    const auto attachment = repo.getById(id, /*withData=*/true);
    if (!attachment) {
        return;
    }

    QPixmap pixmap;
    if (!attachment->data.empty()) {
        pixmap.loadFromData(attachment->data.data(), static_cast<uint>(attachment->data.size()));
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromStdString(attachment->filename));
    dialog.resize(620, 480);

    auto *imageLabel = new QLabel(&dialog);
    imageLabel->setAlignment(Qt::AlignCenter);
    if (pixmap.isNull()) {
        imageLabel->setText("(cannot preview this file)");
    } else {
        imageLabel->setPixmap(
            pixmap.scaled(QSize(840, 560), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    auto *scroll = new QScrollArea(&dialog);
    scroll->setWidget(imageLabel);
    scroll->setWidgetResizable(true);

    auto *deleteButton = new QPushButton("Delete", &dialog);
    deleteButton->setStyleSheet(QString("color:%1;").arg(tokens::critical));
    auto *closeButton = new QPushButton("Close", &dialog);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    buttonRow->addWidget(deleteButton);
    buttonRow->addWidget(closeButton);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(scroll, 1);
    layout->addLayout(buttonRow);

    bool deleted = false;
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(deleteButton, &QPushButton::clicked, &dialog, [&]() {
        if (QMessageBox::question(&dialog, "Delete attachment",
                                  "Delete this attachment permanently?")
            == QMessageBox::Yes) {
            storage::AttachmentRepository r(*db_);
            r.remove(id);
            deleted = true;
            dialog.accept();
        }
    });

    dialog.exec();
    if (deleted) {
        loadAttachments(currentId_);
    }
}

} // namespace pm::app
