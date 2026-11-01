#include "ConnectionSettingsWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

ConnectionSettingsWidget::ConnectionSettingsWidget(AtemOscApp *app, QWidget *parent)
    : QWidget(parent)
    , m_app(app)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // 1. Switcher Connection Group
    auto *grpConn = new QGroupBox(QStringLiteral("ATEM Switcher Connection"), this);
    auto *formConn = new QFormLayout(grpConn);
    formConn->setLabelAlignment(Qt::AlignRight);

    m_lblSwitcherName = new QLabel(QStringLiteral("Not connected"), this);
    QFont boldFont = m_lblSwitcherName->font();
    boldFont.setBold(true);
    m_lblSwitcherName->setFont(boldFont);
    formConn->addRow(QStringLiteral("Model:"), m_lblSwitcherName);

    m_lblStatus = new QLabel(QStringLiteral("Disconnected"), this);
    formConn->addRow(QStringLiteral("Status:"), m_lblStatus);

    m_txtIp = new QLineEdit(this);
    m_txtIp->setPlaceholderText(QStringLiteral("e.g. 192.168.10.240"));
    formConn->addRow(QStringLiteral("Switcher IP Address:"), m_txtIp);

    m_txtNickname = new QLineEdit(this);
    m_txtNickname->setPlaceholderText(QStringLiteral("Optional (required if using multiple switchers)"));
    formConn->addRow(QStringLiteral("OSC Nickname:"), m_txtNickname);

    m_cmbBackend = new QComboBox(this);
    m_cmbBackend->addItem(QStringLiteral("Auto (Recommended)"), static_cast<int>(Atem::BackendType::Auto));
    m_cmbBackend->addItem(QStringLiteral("Official Blackmagic SDK 11.0"), static_cast<int>(Atem::BackendType::OfficialSdk));
    m_cmbBackend->addItem(QStringLiteral("Direct UDP Network Protocol (port 9910)"), static_cast<int>(Atem::BackendType::DirectNetwork));
    formConn->addRow(QStringLiteral("Driver Backend:"), m_cmbBackend);

    auto *btnConnLayout = new QHBoxLayout;
    m_btnConnect = new QPushButton(QStringLiteral("Connect"), this);
    m_btnDisconnect = new QPushButton(QStringLiteral("Disconnect"), this);
    m_btnConnect->setDefault(true);
    btnConnLayout->addWidget(m_btnConnect);
    btnConnLayout->addWidget(m_btnDisconnect);
    btnConnLayout->addStretch();
    formConn->addRow(QString(), btnConnLayout);

    mainLayout->addWidget(grpConn);

    // 2. OSC Feedback Group
    auto *grpOsc = new QGroupBox(QStringLiteral("OSC Feedback Settings"), this);
    auto *formOsc = new QFormLayout(grpOsc);
    formOsc->setLabelAlignment(Qt::AlignRight);

    m_txtOutgoingIp = new QLineEdit(this);
    m_txtOutgoingIp->setPlaceholderText(QStringLiteral("e.g. 127.0.0.1"));
    formOsc->addRow(QStringLiteral("Outgoing IP Address:"), m_txtOutgoingIp);

    m_spnOutgoingPort = new QSpinBox(this);
    m_spnOutgoingPort->setRange(1, 65535);
    m_spnOutgoingPort->setValue(53000);
    formOsc->addRow(QStringLiteral("Outgoing Port:"), m_spnOutgoingPort);

    m_chkInvertTBar = new QCheckBox(QStringLiteral("Invert transition handle (T-Bar) values"), this);
    formOsc->addRow(QString(), m_chkInvertTBar);

    m_chkAutoReconnect = new QCheckBox(QStringLiteral("Automatically reconnect on startup"), this);
    formOsc->addRow(QString(), m_chkAutoReconnect);

    mainLayout->addWidget(grpOsc);
    mainLayout->addStretch();

    // Wire signals
    connect(m_btnConnect, &QPushButton::clicked, this, &ConnectionSettingsWidget::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &ConnectionSettingsWidget::onDisconnectClicked);

    connect(m_txtIp, &QLineEdit::editingFinished, this, &ConnectionSettingsWidget::applySettings);
    connect(m_txtNickname, &QLineEdit::editingFinished, this, &ConnectionSettingsWidget::applySettings);
    connect(m_txtOutgoingIp, &QLineEdit::editingFinished, this, &ConnectionSettingsWidget::applySettings);
    connect(m_spnOutgoingPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &ConnectionSettingsWidget::applySettings);
    connect(m_chkInvertTBar, &QCheckBox::toggled, this, &ConnectionSettingsWidget::applySettings);
    connect(m_chkAutoReconnect, &QCheckBox::toggled, this, &ConnectionSettingsWidget::applySettings);
    connect(m_cmbBackend, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConnectionSettingsWidget::applySettings);

    connect(m_app, &AtemOscApp::switcherUpdated, this, [this](int idx) {
        if (idx == m_currentIndex) {
            updateUiFromConfig();
        }
    });
}

void ConnectionSettingsWidget::setSwitcherIndex(int index) {
    m_currentIndex = index;
    updateUiFromConfig();
}

void ConnectionSettingsWidget::updateUiFromConfig() {
    if (m_currentIndex < 0 || m_currentIndex >= m_app->switchers().size()) {
        setEnabled(false);
        m_lblSwitcherName->setText(QStringLiteral("No switcher selected"));
        m_lblStatus->setText(QStringLiteral("-"));
        return;
    }

    setEnabled(true);
    m_updating = true;

    auto *sw = m_app->switchers()[m_currentIndex];
    m_txtIp->setText(sw->config.ip);
    m_txtNickname->setText(sw->config.nickname);
    m_txtOutgoingIp->setText(sw->config.outgoingIp);
    m_spnOutgoingPort->setValue(sw->config.outgoingPort);
    m_chkInvertTBar->setChecked(sw->config.invertTBar);
    m_chkAutoReconnect->setChecked(sw->config.autoReconnect);

    int bIdx = m_cmbBackend->findData(static_cast<int>(sw->config.backendType));
    if (bIdx >= 0) m_cmbBackend->setCurrentIndex(bIdx);

    if (sw->backend) {
        m_lblSwitcherName->setText(sw->backend->switcherName());
        Atem::ConnectionState st = sw->backend->connectionState();
        if (st == Atem::ConnectionState::Connected) {
            m_lblStatus->setText(QStringLiteral("<font color='#2ecc71'>Connected</font> (via %1)").arg(sw->backend->backendName()));
            m_btnConnect->setEnabled(false);
            m_btnDisconnect->setEnabled(true);
        } else if (st == Atem::ConnectionState::Connecting) {
            m_lblStatus->setText(QStringLiteral("<font color='#f39c12'>Connecting...</font>"));
            m_btnConnect->setEnabled(false);
            m_btnDisconnect->setEnabled(true);
        } else if (st == Atem::ConnectionState::Failed) {
            m_lblStatus->setText(QStringLiteral("<font color='#e74c3c'>Connection failed</font>"));
            m_btnConnect->setEnabled(true);
            m_btnDisconnect->setEnabled(false);
        } else {
            m_lblStatus->setText(QStringLiteral("Disconnected"));
            m_btnConnect->setEnabled(true);
            m_btnDisconnect->setEnabled(false);
        }
    }

    m_updating = false;
}

void ConnectionSettingsWidget::applySettings() {
    if (m_updating || m_currentIndex < 0 || m_currentIndex >= m_app->switchers().size()) {
        return;
    }

    SwitcherConfig cfg;
    cfg.id = m_app->switchers()[m_currentIndex]->config.id;
    cfg.ip = m_txtIp->text().trimmed();
    cfg.nickname = m_txtNickname->text().trimmed();
    cfg.outgoingIp = m_txtOutgoingIp->text().trimmed();
    cfg.outgoingPort = static_cast<quint16>(m_spnOutgoingPort->value());
    cfg.invertTBar = m_chkInvertTBar->isChecked();
    cfg.autoReconnect = m_chkAutoReconnect->isChecked();
    cfg.backendType = static_cast<Atem::BackendType>(m_cmbBackend->currentData().toInt());

    m_app->updateSwitcherConfig(m_currentIndex, cfg);
}

void ConnectionSettingsWidget::onConnectClicked() {
    applySettings();
    if (m_currentIndex >= 0) {
        m_app->connectSwitcher(m_currentIndex);
    }
}

void ConnectionSettingsWidget::onDisconnectClicked() {
    if (m_currentIndex >= 0) {
        m_app->disconnectSwitcher(m_currentIndex);
    }
}
