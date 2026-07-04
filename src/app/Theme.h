#pragma once

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QString>

// The "Digital Fortitude" dark theme (docs/UI-SPEC.md §1.1) as a QPalette + global
// style sheet. Header-only so any widget can read a token if needed.
namespace pm::app {

namespace tokens {
inline const QString deep = "#020617";          // window background
inline const QString surface = "#1E293B";       // cards / fields
inline const QString border = "#334155";        // 1px outlines
inline const QString container = "#171F33";      // hover / pressed fills
inline const QString onSurface = "#DAE2FD";      // primary text
inline const QString muted = "#C3C6D7";          // labels / meta
inline const QString primary = "#B4C5FF";        // primary button fill
inline const QString primaryStrong = "#2563EB";  // accents / selection
inline const QString success = "#4EDEA3";        // success
inline const QString critical = "#EF4444";       // errors / delete
inline const QString warning = "#F59E0B";        // warnings
} // namespace tokens

inline QString styleSheet()
{
    return QString(R"QSS(
        QWidget { background-color: %1; color: %5; font-size: 14px; }
        QLabel#Title { font-size: 24px; font-weight: bold; color: %5; }
        QLabel#Subtitle { color: %6; }
        QLabel#SectionLabel { color: %6; font-family: "JetBrains Mono","Consolas",monospace;
                              letter-spacing: 1px; }
        QLabel#Error { color: %9; }
        QLineEdit {
            background-color: %2; border: 1px solid %3; border-radius: 4px;
            padding: 8px 10px; color: %5; selection-background-color: %8;
        }
        QLineEdit:focus { border: 1px solid %7; }
        QPushButton {
            background-color: %2; border: 1px solid %3; border-radius: 4px;
            padding: 8px 14px; color: %5;
        }
        QPushButton:hover { background-color: %4; }
        QPushButton:disabled { color: %6; border-color: %3; }
        QPushButton#Primary {
            background-color: %7; color: #002A78; border: none; font-weight: bold;
        }
        QPushButton#Primary:hover { background-color: %8; color: white; }
        QPushButton#Chip { padding: 4px 12px; border-radius: 999px; }
        QPushButton#Chip:checked { background-color: %8; color: white; border: 1px solid %8; }
        QWidget#NavRail { background-color: %2; border-right: 1px solid %3; }
        QLabel#NavTitle { font-size: 16px; font-weight: bold; color: %7; background: transparent; }
        QPushButton#NavItem { text-align: left; padding: 9px 14px; border: none; border-radius: 6px;
                              background: transparent; color: %6; }
        QPushButton#NavItem:hover { background-color: %4; color: %5; }
        QPushButton#NavItem:checked { background-color: %4; color: %7; font-weight: bold; }
        QWidget#TopBar { background-color: %2; border-bottom: 1px solid %3; }
        QPushButton#Hamburger { background: transparent; border: none; color: %5; font-size: 18px; }
        QPushButton#Hamburger:hover { color: %7; }
        QComboBox { background-color: %2; border: 1px solid %3; border-radius: 4px; padding: 6px 8px; color: %5; }
        QComboBox QAbstractItemView { background-color: %2; color: %5; selection-background-color: %4; }
        QPlainTextEdit { background-color: %2; border: 1px solid %3; border-radius: 4px; color: %5; }
        QListWidget {
            background-color: %2; border: 1px solid %3; border-radius: 8px; padding: 4px;
        }
        QListWidget::item { padding: 10px; border-radius: 4px; }
        QListWidget::item:selected { background-color: %4; color: %5; }
    )QSS")
        .arg(tokens::deep, tokens::surface, tokens::border, tokens::container,
             tokens::onSurface, tokens::muted, tokens::primary, tokens::primaryStrong,
             tokens::critical);
}

inline void applyTheme(QApplication &app)
{
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(tokens::deep));
    pal.setColor(QPalette::Base, QColor(tokens::surface));
    pal.setColor(QPalette::AlternateBase, QColor(tokens::container));
    pal.setColor(QPalette::Text, QColor(tokens::onSurface));
    pal.setColor(QPalette::WindowText, QColor(tokens::onSurface));
    pal.setColor(QPalette::Button, QColor(tokens::surface));
    pal.setColor(QPalette::ButtonText, QColor(tokens::onSurface));
    pal.setColor(QPalette::Highlight, QColor(tokens::primaryStrong));
    pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
    pal.setColor(QPalette::PlaceholderText, QColor(tokens::muted));
    app.setPalette(pal);
    app.setStyleSheet(styleSheet());
}

} // namespace pm::app
