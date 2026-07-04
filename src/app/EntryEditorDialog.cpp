#include "app/EntryEditorDialog.h"

#include <set>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace pm::app {

namespace {

struct Tmpl
{
    const char *key;
    const char *label;
    model::FieldType type;
    bool secret;
    bool multiline;
};

// Per-type field templates (docs/DATA-MODEL.md §6).
std::vector<Tmpl> templateFor(model::EntryType t)
{
    using FT = model::FieldType;
    switch (t) {
    case model::EntryType::Login:
        return {{"username", "Username", FT::Text, false, false},
                {"password", "Password", FT::Secret, true, false},
                {"url", "Website URL", FT::Url, false, false},
                {"totp", "TOTP secret", FT::Totp, true, false}};
    case model::EntryType::ApiKey:
        return {{"key_id", "Key / Client ID", FT::Text, false, false},
                {"key_secret", "Secret", FT::Secret, true, false},
                {"endpoint", "Endpoint / Base URL", FT::Url, false, false},
                {"environment", "Environment", FT::Text, false, false}};
    case model::EntryType::SecureNote:
        return {{"body", "Note", FT::Multiline, false, true}};
    case model::EntryType::WebAddress:
        return {{"site_name", "Site name", FT::Text, false, false},
                {"url", "Address (URL)", FT::Url, false, false},
                {"category", "Category", FT::Text, false, false}};
    case model::EntryType::SshKey:
        return {{"host", "Host", FT::Text, false, false},
                {"port", "Port", FT::Number, false, false},
                {"username", "Username", FT::Text, false, false},
                {"private_key", "Private key", FT::Multiline, true, true},
                {"public_key", "Public key", FT::Multiline, false, true},
                {"passphrase", "Key passphrase", FT::Secret, true, false}};
    }
    return {};
}

int typeToIndex(model::EntryType t) { return static_cast<int>(t); }
model::EntryType indexToType(int i) { return static_cast<model::EntryType>(i); }

QString fieldValue(const model::Entry &e, const std::string &key)
{
    for (const auto &f : e.fields) {
        if (f.key == key) {
            return QString::fromStdString(f.value);
        }
    }
    return {};
}

} // namespace

EntryEditorDialog::EntryEditorDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Add Entry");
    setMinimumWidth(440);

    typeCombo_ = new QComboBox(this);
    // Order must mirror model::EntryType (typeToIndex/indexToType cast directly).
    typeCombo_->addItems({"Login", "API Key", "Secure Note", "Web Address", "SSH Key"});

    title_ = new QLineEdit(this);
    title_->setPlaceholderText("e.g. GitHub");

    auto *topForm = new QFormLayout;
    topForm->addRow("Type", typeCombo_);
    topForm->addRow("Title", title_);

    fieldsHost_ = new QWidget(this);
    fieldsForm_ = new QFormLayout(fieldsHost_);
    fieldsForm_->setContentsMargins(0, 0, 0, 0);

    auto *notesLabel = new QLabel("Notes", this);
    notesLabel->setObjectName("SectionLabel");
    notes_ = new QPlainTextEdit(this);
    notes_->setFixedHeight(70);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &EntryEditorDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &EntryEditorDialog::reject);
    connect(typeCombo_, &QComboBox::currentIndexChanged, this,
            &EntryEditorDialog::rebuildFields);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(topForm);
    layout->addWidget(fieldsHost_);
    layout->addWidget(notesLabel);
    layout->addWidget(notes_);
    layout->addWidget(buttons);

    rebuildFields(); // initial (Login)
}

model::EntryType EntryEditorDialog::currentType() const
{
    return indexToType(typeCombo_->currentIndex());
}

void EntryEditorDialog::rebuildFields()
{
    // Tear down the previous field set.
    while (fieldsForm_->rowCount() > 0) {
        fieldsForm_->removeRow(0); // deletes the row's widgets
    }
    fields_.clear();

    const model::Entry *prefill = editMode_ ? &base_ : nullptr;

    for (const auto &t : templateFor(currentType())) {
        FieldEditor fe{t.key, t.label, t.type, t.secret, t.multiline, nullptr};

        if (t.multiline) {
            auto *pte = new QPlainTextEdit(this);
            pte->setFixedHeight(80);
            if (prefill) {
                pte->setPlainText(fieldValue(*prefill, t.key));
            }
            fe.editor = pte;
            fieldsForm_->addRow(t.label, pte);
        } else {
            auto *le = new QLineEdit(this);
            if (t.secret) {
                le->setEchoMode(QLineEdit::Password);
            }
            if (prefill) {
                le->setText(fieldValue(*prefill, t.key));
            }
            fe.editor = le;

            if (t.secret) {
                auto *show = new QPushButton("Show", this);
                show->setCheckable(true);
                show->setFixedWidth(64);
                connect(show, &QPushButton::toggled, le, [le, show](bool on) {
                    le->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
                    show->setText(on ? "Hide" : "Show");
                });
                auto *row = new QWidget(this);
                auto *h = new QHBoxLayout(row);
                h->setContentsMargins(0, 0, 0, 0);
                h->addWidget(le, 1);
                h->addWidget(show);
                fieldsForm_->addRow(t.label, row);
            } else {
                fieldsForm_->addRow(t.label, le);
            }
        }
        fields_.push_back(fe);
    }
}

QString EntryEditorDialog::valueOf(const FieldEditor &fe) const
{
    if (fe.multiline) {
        return static_cast<QPlainTextEdit *>(fe.editor)->toPlainText();
    }
    return static_cast<QLineEdit *>(fe.editor)->text();
}

void EntryEditorDialog::setEntry(const pm::model::Entry &entry)
{
    editMode_ = true;
    base_ = entry;
    setWindowTitle("Edit Entry");

    title_->setText(QString::fromStdString(entry.title));
    if (entry.notes) {
        notes_->setPlainText(QString::fromStdString(*entry.notes));
    }

    typeCombo_->blockSignals(true);
    typeCombo_->setCurrentIndex(typeToIndex(entry.type));
    typeCombo_->blockSignals(false);
    // Type stays changeable on edit (Opsi B); accept() warns before dropping
    // fields that don't exist in the new type.

    rebuildFields();
}

void EntryEditorDialog::accept()
{
    if (title_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Entry", "A title is required.");
        return;
    }

    // Opsi B: if editing and the type changed, warn about original fields that
    // have no place in the new type's template and will be dropped on save.
    if (editMode_ && currentType() != base_.type) {
        std::set<std::string> newKeys;
        for (const auto &t : templateFor(currentType())) {
            newKeys.insert(t.key);
        }
        QStringList lost;
        for (const auto &f : base_.fields) {
            if (!f.value.empty() && newKeys.find(f.key) == newKeys.end()) {
                lost << QString::fromStdString(f.label);
            }
        }
        if (!lost.isEmpty()) {
            const auto answer = QMessageBox::warning(
                this, "Ubah tipe entry",
                QString("Mengubah tipe akan MENGHAPUS field yang tidak ada di tipe "
                        "baru:\n\n• %1\n\nLanjutkan?")
                    .arg(lost.join("\n• ")),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
    }

    QDialog::accept();
}

pm::model::Entry EntryEditorDialog::result() const
{
    pm::model::Entry e = base_; // keeps id / createdAt on edit
    e.type = currentType();
    e.title = title_->text().trimmed().toStdString();

    const QString notes = notes_->toPlainText().trimmed();
    e.notes = notes.isEmpty() ? std::optional<std::string>{}
                              : std::optional<std::string>(notes.toStdString());

    e.fields.clear();
    int position = 0;
    for (const auto &fe : fields_) {
        const QString v = valueOf(fe);
        if (v.isEmpty()) {
            continue;
        }
        e.fields.push_back({0, fe.key, fe.label, v.toStdString(), fe.type, fe.secret,
                            position++});
    }
    return e;
}

} // namespace pm::app
