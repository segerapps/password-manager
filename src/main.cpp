#include <exception>

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QMessageBox>
#include <QString>

#include "app/MainWindow.h"
#include "app/Theme.h"
#include "core/crypto/Crypto.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Password Manager");
    QApplication::setOrganizationName("PasswordManager");
    QApplication::setWindowIcon(QIcon(":/app_icon.ico"));

    // Bundled OFL fonts: Hanken Grotesk for UI text, JetBrains Mono for secrets.
    QFontDatabase::addApplicationFont(":/fonts/HankenGrotesk-Variable.ttf");
    QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Bold.ttf");
    app.setFont(QFont("Hanken Grotesk"));

    // Cryptography must be available before anything touches the vault.
    try {
        pm::crypto::init();
    } catch (const std::exception &e) {
        QMessageBox::critical(nullptr, "Password Manager",
                              QString("Failed to initialize cryptography:\n%1").arg(e.what()));
        return 1;
    }

    pm::app::applyTheme(app);

    pm::app::MainWindow window;
    window.show();

    return app.exec();
}
