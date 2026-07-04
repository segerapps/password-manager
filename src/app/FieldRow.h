#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;
class QPushButton;

namespace pm::app {

// One labelled field in the detail pane: a mono uppercase label above a read-only
// value, with trailing actions — reveal (secret only), copy, and open-in-browser
// (URL only). With `multiline` the value is shown as a 2-line read-only box (used
// for Notes). See docs/UI-SPEC.md §3.3.
class FieldRow : public QWidget
{
    Q_OBJECT

public:
    FieldRow(const QString &label, const QString &value, bool secret, bool isUrl,
             bool multiline = false, QWidget *parent = nullptr);

private slots:
    void toggleReveal();

private:
    QString value_;
    bool secret_;
    QLineEdit *field_ = nullptr; // null in multiline mode
    QPushButton *revealButton_ = nullptr;
};

} // namespace pm::app
