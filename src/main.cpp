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
    app.setApplicationVersion(QStringLiteral(ATEMREMOTEOSC_VERSION));
    app.setOrganizationName(QStringLiteral("AtemRemoteOsc"));

    theme::applyToApplication(app);

    QCommandLineParser cli;
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption shotOpt(QStringLiteral("screenshot"), QStringLiteral("Capture the main window, then quit (tests)."), QStringLiteral("capture.png"));
    cli.addOption(shotOpt);
    QCommandLineOption delayOpt(QStringLiteral("screenshot-delay"), QStringLiteral("Milliseconds to wait before the capture (with --screenshot)."), QStringLiteral("ms"), QStringLiteral("800"));
    cli.addOption(delayOpt);
    QCommandLineOption pageOpt(QStringLiteral("page"), QStringLiteral("Page to show first (0 Switchers, 1 Network, 2 Addresses, 3 Log, 4 Settings)."), QStringLiteral("n"), QStringLiteral("0"));
    cli.addOption(pageOpt);
    QCommandLineOption sizeOpt(QStringLiteral("size"), QStringLiteral("Window size, e.g. 1180x600 (with --screenshot)."), QStringLiteral("WxH"));
    cli.addOption(sizeOpt);
    cli.process(app);

    AtemRemoteOscApp oscApp;
    if (!oscApp.initialize()) {
        return 1;
    }

    MainWindow mainWindow(&oscApp);
    mainWindow.showPage(cli.value(pageOpt).toInt());
    if (cli.isSet(sizeOpt)) {
        const QStringList wh = cli.value(sizeOpt).split(QLatin1Char('x'));
        if (wh.size() == 2) mainWindow.resize(wh[0].toInt(), wh[1].toInt());
    }
    mainWindow.show();

    if (cli.isSet(shotOpt)) {
        const QString path = cli.value(shotOpt);
        QTimer::singleShot(cli.value(delayOpt).toInt(), &mainWindow, [&mainWindow, path]() {
            mainWindow.grab().save(path);
            QApplication::quit();
        });
    }

    return app.exec();
}
