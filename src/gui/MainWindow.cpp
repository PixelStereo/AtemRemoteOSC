#include "MainWindow.h"
#include "About.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QSettings>
#include <QPushButton>
#include <QVBoxLayout>

MainWindow::MainWindow(AtemRemoteOscApp *app, QWidget *parent)
    : QMainWindow(parent)
    , m_app(app)
{
    setupUi();
    setupMenus();

    QSettings settings("AtemRemoteOsc", "AtemRemoteOsc");
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());

    for (const auto &entry : m_app->logHistory()) m_logConsole->appendLogAt(entry.first, entry.second);
    connect(m_app, &AtemRemoteOscApp::logMessage, m_logConsole, &LogConsoleWidget::appendLog);

    m_logConsole->setLogDebugOsc(m_app->logDebugOsc());
    connect(m_logConsole, &LogConsoleWidget::logDebugOscChanged, m_app, &AtemRemoteOscApp::setLogDebugOsc);

    connect(m_logConsole, &LogConsoleWidget::messageLogged, m_status, &StatusLine::setMessage);
    connect(m_logConsole, &LogConsoleWidget::countsChanged, this, [this] {
        m_status->setAlerts(m_logConsole->count(LogConsoleWidget::Error), m_logConsole->count(LogConsoleWidget::Warning));
    });
    connect(m_status, &StatusLine::alertsClicked, this, [this] { showPage(PageLog); });
    connect(m_status, &StatusLine::messageClicked, this, [this] { showPage(PageLog); });

    for (auto signal : {&AtemRemoteOscApp::oscServerStateChanged, &AtemRemoteOscApp::oscQueryStateChanged})
        connect(m_app, signal, this, [this](bool, quint16) { updateSummary(); });
    connect(m_app, &AtemRemoteOscApp::switcherListChanged, this, &MainWindow::updateSummary);
    connect(m_app, &AtemRemoteOscApp::switcherUpdated, this, &MainWindow::updateSummary);
    m_status->setAlerts(m_logConsole->count(LogConsoleWidget::Error), m_logConsole->count(LogConsoleWidget::Warning));
    updateSummary();

    if (!m_app->switchers().isEmpty()) m_connectionWidget->setSwitcherIndex(m_switcherList->selectedIndex() < 0 ? 0 : m_switcherList->selectedIndex());
}

MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings("AtemRemoteOsc", "AtemRemoteOsc");
    settings.setValue("mainWindowGeometry", saveGeometry());
    m_app->saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::showPage(int page)
{
    m_pages->setCurrentIndex(page);
    if (auto *b = m_nav.button(page)) b->setChecked(true);
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("AtemRemoteOsc"));
    setMinimumSize(900, 520);
    resize(1180, 600); // wide and low: a control strip, not a document

    auto *root = new QWidget(this);
    auto *column = new QVBoxLayout(root);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    m_status = new StatusLine(root);
    column->addWidget(m_status);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    column->addLayout(body, 1);

    // Navigation rail
    auto *rail = new QFrame(root);
    rail->setObjectName(QStringLiteral("rail"));
    rail->setFixedWidth(156);
    auto *railColumn = new QVBoxLayout(rail);
    railColumn->setContentsMargins(0, 12, 0, 12);
    railColumn->setSpacing(2);
    const char *names[] = {"Switchers", "Network", "Addresses", "Log", "Settings"};
    for (int i = 0; i < 5; ++i) {
        auto *b = new QPushButton(QString::fromLatin1(names[i]), rail);
        b->setObjectName(QStringLiteral("nav"));
        b->setCheckable(true);
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_nav.addButton(b, i);
        railColumn->addWidget(b);
    }
    railColumn->addStretch();
    auto *version = new QLabel(QStringLiteral("v%1").arg(QApplication::applicationVersion()), rail);
    version->setObjectName(QStringLiteral("caption"));
    version->setContentsMargins(18, 0, 0, 0);
    railColumn->addWidget(version);
    body->addWidget(rail);

    // Pages
    m_pages = new QStackedWidget(root);
    auto *switchers = new QWidget(m_pages);
    auto *switchersColumn = new QVBoxLayout(switchers);
    switchersColumn->setContentsMargins(24, 18, 24, 18);
    switchersColumn->setSpacing(0);
    m_switcherList = new SwitcherListWidget(m_app, switchers);
    m_connectionWidget = new ConnectionSettingsWidget(m_app, switchers);
    switchersColumn->addWidget(m_switcherList);
    switchersColumn->addWidget(m_connectionWidget, 1);

    m_network = new NetworkPanel(m_app, m_pages);
    m_addressBrowser = new OscAddressBrowserWidget(m_app->addressRegistry(), m_pages);
    m_logConsole = new LogConsoleWidget(m_pages);
    m_settings = new SettingsPanel(m_pages);
    for (QWidget *page : {switchers, static_cast<QWidget *>(m_network), static_cast<QWidget *>(m_addressBrowser),
                          static_cast<QWidget *>(m_logConsole), static_cast<QWidget *>(m_settings)})
        m_pages->addWidget(page);
    body->addWidget(m_pages, 1);
    setCentralWidget(root);

    connect(&m_nav, &QButtonGroup::idClicked, this, [this](int id) { m_pages->setCurrentIndex(id); });
    connect(m_switcherList, &SwitcherListWidget::selectionChanged, m_connectionWidget, &ConnectionSettingsWidget::setSwitcherIndex);
    connect(m_connectionWidget, &ConnectionSettingsWidget::addRequested, m_switcherList, &SwitcherListWidget::addSwitcher);
    showPage(PageSwitchers);
}

void MainWindow::setupMenus()
{
    auto *file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(QStringLiteral("Add a Switcher"), QKeySequence::New, m_switcherList, &SwitcherListWidget::addSwitcher);
    file->addSeparator();
    auto *quit = file->addAction(QStringLiteral("Quit"), QKeySequence::Quit, this, &QWidget::close);
    quit->setMenuRole(QAction::QuitRole);

    auto *go = menuBar()->addMenu(QStringLiteral("&Go"));
    const char *names[] = {"Switchers", "Network", "Addresses", "Log", "Settings"};
    for (int i = 0; i < 5; ++i)
        go->addAction(QString::fromLatin1(names[i]), QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + i)), this,
                      [this, i] { showPage(i); });

    auto *help = menuBar()->addMenu(QStringLiteral("&Help"));
    auto *about = help->addAction(QStringLiteral("About AtemRemoteOsc"), this, [this] { showAbout(this); });
    about->setMenuRole(QAction::AboutRole);
}

void MainWindow::updateSummary()
{
    auto dot = [](const QColor &c) { return QStringLiteral("<font color='%1'>●</font>&nbsp;").arg(c.name()); };
    const QColor ok(0x5f, 0xc9, 0x7a), off(0x9a, 0x9a, 0xa3), bad(0xff, 0x8f, 0x82);

    QString text = dot(m_app->isOscServerListening() ? ok : bad)
                   + (m_app->isOscServerListening() ? QStringLiteral("OSC %1").arg(m_app->incomingPort()) : QStringLiteral("OSC off"));
    text += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;");
    text += dot(m_app->isOscQueryRunning() ? ok : (m_app->oscQueryEnabled() ? bad : off))
            + (m_app->isOscQueryRunning() ? QStringLiteral("OSCQuery %1").arg(m_app->oscQueryPort()) : QStringLiteral("OSCQuery off"));

    int connected = 0;
    const auto &sw = m_app->switchers();
    for (auto *s : sw)
        if (s->backend && s->backend->connectionState() == Atem::ConnectionState::Connected) ++connected;
    text += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;");
    text += dot(connected > 0 ? ok : off)
            + (sw.isEmpty() ? QStringLiteral("no switcher") : QStringLiteral("%1/%2 connected").arg(connected).arg(sw.size()));
    m_status->setSummary(text);
}
