#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QTimer;

namespace pm::app {

// Detail-pane row for a TOTP field: shows the live 6-digit code (grouped
// "123 456") with a rollover countdown, refreshing every second. The stored
// base32 secret itself is never displayed here — it is only visible in the
// editor. Copy copies the code valid at click time.
class TotpRow : public QWidget
{
    Q_OBJECT

public:
    TotpRow(const QString &label, const QString &base32Secret,
            QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QString currentCode() const; // ungrouped, for the clipboard

    QString secret_;
    QLabel *code_ = nullptr;
    QLabel *countdown_ = nullptr;
    QTimer *timer_ = nullptr;
};

} // namespace pm::app
