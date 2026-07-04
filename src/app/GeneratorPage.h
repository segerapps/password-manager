#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSlider;

namespace pm::app {

// A top-level password generator (docs/UI-SPEC.md §3.5). Uses libsodium's
// unbiased CSPRNG (randombytes_uniform) to pick characters.
class GeneratorPage : public QWidget
{
    Q_OBJECT

public:
    explicit GeneratorPage(QWidget *parent = nullptr);

private slots:
    void generate();

private:
    QSlider *length_ = nullptr;
    QLabel *lengthLabel_ = nullptr;
    QCheckBox *upper_ = nullptr;
    QCheckBox *lower_ = nullptr;
    QCheckBox *digits_ = nullptr;
    QCheckBox *symbols_ = nullptr;
    QCheckBox *ambiguous_ = nullptr;
    QLineEdit *output_ = nullptr;
};

} // namespace pm::app
