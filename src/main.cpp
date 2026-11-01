#include <QApplication>
#include <QStyleFactory>
#include "app/AtemOscApp.h"
#include "gui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("atemOSC"));
    app.setApplicationVersion(QStringLiteral("4.0.0"));
    app.setOrganizationName(QStringLiteral("atemOSC"));
    app.setOrganizationDomain(QStringLiteral("atemosc.com"));

    // Modern clean styling
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    AtemOscApp oscApp;
    if (!oscApp.initialize()) {
        return 1;
    }

    MainWindow mainWindow(&oscApp);
    mainWindow.show();

    return app.exec();
}
