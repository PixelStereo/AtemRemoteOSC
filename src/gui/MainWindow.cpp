#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QSettings>
#include <QCloseEvent>

MainWindow::MainWindow(AtemRemoteOscApp *app, QWidget *parent)
    : QMainWindow(parent)
    , m_app(app)
{
    setupUi();

    QSettings settings("AtemRemoteOsc", "AtemRemoteOsc");
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());

    for (const auto &entry : m_app->logHistory()) {
        m_logConsole->appendLogAt(entry.first, entry.second);
    }
    connect(m_app, &AtemRemoteOscApp::logMessage, m_logConsole, &LogConsoleWidget::appendLog);
    connect(m_app, &AtemRemoteOscApp::oscServerStateChanged, this, &MainWindow::onOscServerStateChanged);

    m_logConsole->setLogDebugOsc(m_app->logDebugOsc());
    connect(m_logConsole, &LogConsoleWidget::logDebugOscChanged, m_app, &AtemRemoteOscApp::setLogDebugOsc);

    // Initial server state
    updateServerStatusUi(m_app->isOscServerListening(), m_app->incomingPort());

    // Select first switcher if available
    if (!m_app->switchers().isEmpty()) {
        m_connectionWidget->setSwitcherIndex(0);
    }
}

MainWindow::~MainWindow() {
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QSettings settings("AtemRemoteOsc", "AtemRemoteOsc");
    settings.setValue("mainWindowGeometry", saveGeometry());
    m_app->saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUi() {
    setWindowTitle(QStringLiteral("AtemRemoteOsc – ATEM Switcher OSC Proxy"));
    setMinimumSize(900, 680);
    resize(1040, 760);

    // 1. Toolbar for global Incoming OSC Port
    auto *toolbar = addToolBar(QStringLiteral("OSC Server"));
    toolbar->setMovable(false);

    auto *portContainer = new QWidget(this);
    auto *portLayout = new QHBoxLayout(portContainer);
    portLayout->setContentsMargins(8, 4, 8, 4);
    portLayout->setSpacing(8);

    portLayout->addWidget(new QLabel(QStringLiteral("Incoming OSC Listening Port:"), this));

    m_spnIncomingPort = new QSpinBox(this);
    m_spnIncomingPort->setRange(1024, 65535);
    m_spnIncomingPort->setValue(m_app->incomingPort());
    portLayout->addWidget(m_spnIncomingPort);

    m_btnApplyPort = new QPushButton(QStringLiteral("Restart Listener"), this);
    portLayout->addWidget(m_btnApplyPort);

    m_lblServerStatus = new QLabel(this);
    portLayout->addWidget(m_lblServerStatus);

    portLayout->addSpacing(16);
    m_chkOscQuery = new QCheckBox(QStringLiteral("OSCQuery, HTTP port:"), this);
    m_chkOscQuery->setToolTip(QStringLiteral("Publishes the OSC addresses and their values over HTTP and announces them on the network, "
                                             "so that OSCQuery clients find AtemRemoteOsc by themselves."));
    m_chkOscQuery->setChecked(m_app->oscQueryEnabled());
    portLayout->addWidget(m_chkOscQuery);
    m_spnOscQueryPort = new QSpinBox(this);
    m_spnOscQueryPort->setRange(1024, 65535);
    m_spnOscQueryPort->setValue(m_app->oscQueryPort());
    portLayout->addWidget(m_spnOscQueryPort);
    m_lblOscQueryStatus = new QLabel(this);
    portLayout->addWidget(m_lblOscQueryStatus);
    portLayout->addStretch();

    toolbar->addWidget(portContainer);

    // 2. Central Widget with Splitters
    m_mainSplitter = new QSplitter(Qt::Vertical, this);

    m_topSplitter = new QSplitter(Qt::Horizontal, m_mainSplitter);

    // Left Panel: Switcher List
    m_switcherList = new SwitcherListWidget(m_app, m_topSplitter);
    m_switcherList->setMinimumWidth(200);
    m_topSplitter->addWidget(m_switcherList);

    // Right Panel: Tabs
    m_tabWidget = new QTabWidget(m_topSplitter);

    m_connectionWidget = new ConnectionSettingsWidget(m_app, m_tabWidget);
    m_tabWidget->addTab(m_connectionWidget, QStringLiteral("Connection"));

    m_addressBrowser = new OscAddressBrowserWidget(m_app->addressRegistry(), m_tabWidget);
    m_tabWidget->addTab(m_addressBrowser, QStringLiteral("OSC Addresses"));

    m_topSplitter->addWidget(m_tabWidget);
    m_topSplitter->setStretchFactor(0, 1);
    m_topSplitter->setStretchFactor(1, 3);

    m_mainSplitter->addWidget(m_topSplitter);

    // Bottom Panel: Log Console
    m_logConsole = new LogConsoleWidget(m_mainSplitter);
    m_mainSplitter->addWidget(m_logConsole);

    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_mainSplitter);

    // 3. Status Bar
    statusBar()->showMessage(QStringLiteral("Ready"));

    // Wire events
    connect(m_switcherList, &SwitcherListWidget::selectionChanged, this, &MainWindow::onSwitcherSelected);
    connect(m_btnApplyPort, &QPushButton::clicked, this, &MainWindow::onApplyPortClicked);
    connect(m_chkOscQuery, &QCheckBox::toggled, this, &MainWindow::onOscQueryChanged);
    connect(m_spnOscQueryPort, &QSpinBox::editingFinished, this, &MainWindow::onOscQueryChanged);
    connect(m_app, &AtemRemoteOscApp::oscQueryStateChanged, this, [this](bool running, quint16 port) {
        m_lblOscQueryStatus->setText(running ? QStringLiteral("<font color='#2ecc71'>●</font> HTTP %1").arg(port)
                                             : QStringLiteral("<font color='#9a9aa3'>●</font> off"));
    });
    m_lblOscQueryStatus->setText(m_app->isOscQueryRunning() ? QStringLiteral("<font color='#2ecc71'>●</font> HTTP %1").arg(m_app->oscQueryPort())
                                                            : QStringLiteral("<font color='#9a9aa3'>●</font> off"));
}

void MainWindow::onSwitcherSelected(int index) {
    m_connectionWidget->setSwitcherIndex(index);
}

void MainWindow::onApplyPortClicked() {
    quint16 port = static_cast<quint16>(m_spnIncomingPort->value());
    m_app->setIncomingPort(port);
}

void MainWindow::onOscQueryChanged() {
    m_app->setOscQuery(m_chkOscQuery->isChecked(), static_cast<quint16>(m_spnOscQueryPort->value()));
}

void MainWindow::onOscServerStateChanged(bool listening, quint16 port) {
    updateServerStatusUi(listening, port);
}

void MainWindow::updateServerStatusUi(bool listening, quint16 port) {
    if (listening) {
        m_lblServerStatus->setText(QStringLiteral("<font color='#2ecc71'>●</font> <b>Listening on UDP %1</b>").arg(port));
    } else {
        m_lblServerStatus->setText(QStringLiteral("<font color='#e74c3c'>●</font> <b>Not Listening</b>"));
    }
}
