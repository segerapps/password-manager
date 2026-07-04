#pragma once

#include <string>
#include <vector>

#include <QDialog>

#include "core/model/Entry.h"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QFormLayout;
class QWidget;

namespace pm::app {

// Add or edit any entry type (Login / API Key / Secure Note / Web Address). The
// field set is rebuilt from the per-type template (docs/DATA-MODEL.md §6) when the
// type changes. Preserves id / createdAt on edit; timestamps finalised by caller.
class EntryEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EntryEditorDialog(QWidget *parent = nullptr);

    void setEntry(const pm::model::Entry &entry); // edit mode: fixes type + prefills
    pm::model::Entry result() const;

private slots:
    void accept() override;
    void rebuildFields();

private:
    // One dynamically-built field editor.
    struct FieldEditor
    {
        std::string key;
        std::string label;
        model::FieldType type;
        bool secret;
        bool multiline;
        QWidget *editor; // QLineEdit* or QPlainTextEdit*
    };

    model::EntryType currentType() const;
    QString valueOf(const FieldEditor &fe) const;

    bool editMode_ = false;
    model::Entry base_; // carries id / createdAt across an edit

    QComboBox *typeCombo_ = nullptr;
    QLineEdit *title_ = nullptr;
    QWidget *fieldsHost_ = nullptr;
    QFormLayout *fieldsForm_ = nullptr;
    QPlainTextEdit *notes_ = nullptr;
    std::vector<FieldEditor> fields_;
};

} // namespace pm::app
