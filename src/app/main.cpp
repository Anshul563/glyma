#include <QApplication>

#include "glyma/app/main_window.hpp"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Glyma"));
    QApplication::setApplicationDisplayName(QStringLiteral("Glyma"));
    QApplication::setOrganizationName(QStringLiteral("Glyma"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setStyle(QStringLiteral("Fusion"));

    glyma::app::MainWindow window;
    window.show();
    return application.exec();
}
