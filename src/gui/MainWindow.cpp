#include "MainWindow.h"
#include "About.h"
#include <QAction>
#include <QCloseEvent>
#include <QMenuBar>
#include <QSettings>

MainWindow::MainWindow(AtemRemoteOscApp *app, QWidget *parent)
    : QMainWindow(parent)
    , m_app(app)
{
    setupUi();
    setupMenus();

    QSettings settings("AtemRemoteOsc", "AtemRemoteOsc");
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    showConsole(settings.value("consoleVisible", true).toBool());

    for (const auto &entry : m_app->logHistory()) m_logConsole->appendLogAt(entry.first, entry.second);
    connect(m_app, &AtemRemoteOscApp::logMessage, m_logConsole, &LogConsoleWidget::appendLog);

    m_logConsole->setLogDebugOsc(m_app->logDebugOsc());
    connect(m_logConsole, &LogConsoleWidget::logDebugOscChanged, m_app, &AtemRemoteOscApp::setLogDebugOsc);

    connect(m_logConsole, &LogConsoleWidget::messageLogged, m_status, &StatusLine::setMessage);
    connect(m_logConsole, &LogConsoleWidget::countsChanged, this, [this] {
        m_status->setAlerts(m_logConsole->count(LogConsoleWidget::Error), m_logConsole->count(LogConsoleWidget::Warning));
    });
    connect(m_status, &StatusLine::alertsClicked, this, [this] { showConsole(true); });

    for (auto signal : {&AtemRemoteOscApp::oscServerStateChanged, &AtemRemoteOscApp::oscQueryStateChanged})
        connect(m_app, signal, this, [this](bool, quint16) { updateSummary(); });
    connect(m_app, &AtemRemoteOscApp::switcherListChanged, this, &MainWindow::updateSummary);
    connect(m_app, &AtemRemoteOscApp::switcherUpdated, this, &MainWindow::updateSummary);
    m_status->setAlerts(m_logConsole->count(LogConsoleWidget::Error), m_logConsole->count(LogConsoleWidget::Warning));
    updateSummary();

    if (!m_app->switchers().isEmpty()) m_connectionWidget->setSwitcherIndex(0);
}

MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings("AtemRemoteOsc", "AtemRemoteOsc");
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("consoleVisible", m_consoleDock->isVisible());
    m_app->saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::showConsole(bool show) { m_consoleDock->setVisible(show); }

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("AtemRemoteOsc"));
    setMinimumSize(820, 560);
    resize(1020, 720);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    m_switcherList = new SwitcherListWidget(m_app, m_splitter);
    m_switcherList->setMinimumWidth(220);
    m_switcherList->setMaximumWidth(360);

    m_tabs = new QTabWidget(m_splitter);
    m_tabs->setDocumentMode(false);
    m_connectionWidget = new ConnectionSettingsWidget(m_app, m_tabs);
    m_network = new NetworkPanel(m_app, m_tabs);
    m_addressBrowser = new OscAddressBrowserWidget(m_app->addressRegistry(), m_tabs);
    m_settings = new SettingsPanel(m_tabs);
    m_tabs->addTab(m_connectionWidget, QStringLiteral("Switcher"));
    m_tabs->addTab(m_network, QStringLiteral("Network"));
    m_tabs->addTab(m_addressBrowser, QStringLiteral("Addresses"));
    m_tabs->addTab(m_settings, QStringLiteral("Settings"));

    m_splitter->addWidget(m_switcherList);
    m_splitter->addWidget(m_tabs);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({260, 760});
    setCentralWidget(m_splitter);

    m_consoleDock = new QDockWidget(QStringLiteral("Console"), this);
    m_consoleDock->setObjectName(QStringLiteral("console"));
    m_consoleDock->setFeatures(QDockWidget::DockWidgetClosable);
    m_logConsole = new LogConsoleWidget(m_consoleDock);
    m_consoleDock->setWidget(m_logConsole);
    addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);
    resizeDocks({m_consoleDock}, {170}, Qt::Vertical);

    m_status = new StatusLine(this);
    setStatusBar(m_status);

    connect(m_switcherList, &SwitcherListWidget::selectionChanged, m_connectionWidget, &ConnectionSettingsWidget::setSwitcherIndex);
    connect(m_connectionWidget, &ConnectionSettingsWidget::addRequested, m_switcherList, &SwitcherListWidget::addSwitcher);
}

void MainWindow::setupMenus()
{
    auto *file = menuBar()->addMenu(QStringLiteral("&File"));
    auto *add = file->addAction(QStringLiteral("Add a Switcher"), QKeySequence::New, m_switcherList, &SwitcherListWidget::addSwitcher);
    add->setStatusTip(QStringLiteral("A new switcher; nothing connects until you press Connect"));
    file->addSeparator();
    auto *quit = file->addAction(QStringLiteral("Quit"), QKeySequence::Quit, this, &QWidget::close);
    quit->setMenuRole(QAction::QuitRole);

    auto *view = menuBar()->addMenu(QStringLiteral("&View"));
    auto *console = m_consoleDock->toggleViewAction();
    console->setText(QStringLiteral("Console"));
    console->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    view->addAction(console);

    auto *help = menuBar()->addMenu(QStringLiteral("&Help"));
    auto *about = help->addAction(QStringLiteral("About AtemRemoteOsc"), this, [this] { showAbout(this); });
    about->setMenuRole(QAction::AboutRole);
}

void MainWindow::updateSummary()
{
    auto dot = [](const QColor &c) { return QStringLiteral("<font color='%1'>●</font> ").arg(c.name()); };
    const QColor ok(0x5f, 0xc9, 0x7a), off(0x9a, 0x9a, 0xa3), bad(0xff, 0x8f, 0x82);

    QString text;
    text += dot(m_app->isOscServerListening() ? ok : bad)
            + (m_app->isOscServerListening() ? QStringLiteral("OSC in, UDP %1").arg(m_app->incomingPort()) : QStringLiteral("OSC in off"));
    text += QStringLiteral("&nbsp;&nbsp;&nbsp;");
    text += dot(m_app->isOscQueryRunning() ? ok : (m_app->oscQueryEnabled() ? bad : off))
            + (m_app->isOscQueryRunning() ? QStringLiteral("OSCQuery, HTTP %1").arg(m_app->oscQueryPort()) : QStringLiteral("OSCQuery off"));

    int connected = 0;
    const auto &sw = m_app->switchers();
    for (auto *s : sw)
        if (s->backend && s->backend->connectionState() == Atem::ConnectionState::Connected) ++connected;
    text += QStringLiteral("&nbsp;&nbsp;&nbsp;");
    text += dot(connected > 0 ? ok : off)
            + (sw.isEmpty() ? QStringLiteral("no switcher") : QStringLiteral("%1 of %2 switcher%3 connected").arg(connected).arg(sw.size()).arg(sw.size() > 1 ? "s" : ""));
    m_status->setSummary(text);
}
