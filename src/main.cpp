#include <QApplication>
#include <QCommandLineParser>
#include <QPixmap>
#include <QTimer>
#include "app/AtemRemoteOscApp.h"
#include "gui/MainWindow.h"
#include "gui/Theme.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("AtemRemoteOsc"));
    app.setApplicationVersion(QStringLiteral("5.0.1"));
    app.setOrganizationName(QStringLiteral("AtemRemoteOsc"));

    theme::applyToApplication(app);

    QCommandLineParser cli;
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption shotOpt(QStringLiteral("screenshot"), QStringLiteral("Capture the main window, then quit (tests)."), QStringLiteral("capture.png"));
    cli.addOption(shotOpt);
    cli.process(app);

    AtemRemoteOscApp oscApp;
    if (!oscApp.initialize()) {
        return 1;
    }

    MainWindow mainWindow(&oscApp);
    mainWindow.show();

    if (cli.isSet(shotOpt)) {
        const QString path = cli.value(shotOpt);
        QTimer::singleShot(800, &mainWindow, [&mainWindow, path]() {
            mainWindow.grab().save(path);
            QApplication::quit();
        });
    }

    return app.exec();
}
