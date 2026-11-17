#include "ConnectionSettingsWidget.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

ConnectionSettingsWidget::ConnectionSettingsWidget(AtemRemoteOscApp *app, QWidget *parent)
    : QWidget(parent)
    , m_app(app)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    outer->addWidget(m_stack);

    // Page 0: nothing to show yet
    auto *empty = new QWidget(m_stack);
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->addStretch();
    auto *emptyTitle = new QLabel(QStringLiteral("Connect an ATEM switcher"), empty);
    emptyTitle->setObjectName(QStringLiteral("title"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto *emptyText = new QLabel(QStringLiteral("Add a switcher, type its IP address and press Connect.\n"
                                                "Nothing connects until you ask for it."), empty);
    emptyText->setObjectName(QStringLiteral("hint"));
    emptyText->setAlignment(Qt::AlignCenter);
    auto *btnAdd = new QPushButton(QStringLiteral("Add a switcher"), empty);
    btnAdd->setProperty("role", "go");
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addSpacing(4);
    emptyLayout->addWidget(emptyText);
    emptyLayout->addSpacing(12);
    emptyLayout->addWidget(btnAdd, 0, Qt::AlignHCenter);
    emptyLayout->addStretch();
    m_stack->addWidget(empty);
    connect(btnAdd, &QPushButton::clicked, this, &ConnectionSettingsWidget::addRequested);

    // Page 1: the switcher
    auto *page = new QWidget(m_stack);
    auto *mainLayout = new QVBoxLayout(page);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(12);

    auto *head = new QHBoxLayout;
    m_lblSwitcherName = new QLabel(QStringLiteral("Switcher"), page);
    m_lblSwitcherName->setObjectName(QStringLiteral("title"));
    head->addWidget(m_lblSwitcherName);
    head->addStretch();
    m_lblPill = new QLabel(page);
    m_lblPill->setObjectName(QStringLiteral("pill"));
    head->addWidget(m_lblPill);
    mainLayout->addLayout(head);

    auto *grpConn = new QGroupBox(QStringLiteral("Switcher"), page);
    grpConn->setObjectName(QStringLiteral("section"));
    auto *formConn = new QFormLayout(grpConn);
    formConn->setLabelAlignment(Qt::AlignRight);

    m_txtIp = new QLineEdit(grpConn);
    m_txtIp->setPlaceholderText(QStringLiteral("IP address, e.g. 192.168.10.240"));
    formConn->addRow(QStringLiteral("Address"), m_txtIp);

    m_txtNickname = new QLineEdit(grpConn);
    m_txtNickname->setPlaceholderText(QStringLiteral("Optional; needed to tell several switchers apart in OSC"));
    formConn->addRow(QStringLiteral("OSC name"), m_txtNickname);

    auto *btnRow = new QHBoxLayout;
    m_btnConnect = new QPushButton(QStringLiteral("Connect"), grpConn);
    m_btnConnect->setDefault(true);
    m_btnConnect->setProperty("role", "go");
    m_btnDisconnect = new QPushButton(QStringLiteral("Disconnect"), grpConn);
    btnRow->addWidget(m_btnConnect);
    btnRow->addWidget(m_btnDisconnect);
    btnRow->addStretch();
    formConn->addRow(QString(), btnRow);
    mainLayout->addWidget(grpConn);

    auto *grpOsc = new QGroupBox(QStringLiteral("OSC feedback"), page);
    grpOsc->setObjectName(QStringLiteral("section"));
    auto *formOsc = new QFormLayout(grpOsc);
    formOsc->setLabelAlignment(Qt::AlignRight);

    m_txtOutgoingIp = new QLineEdit(grpOsc);
    m_txtOutgoingIp->setPlaceholderText(QStringLiteral("e.g. 127.0.0.1"));
    formOsc->addRow(QStringLiteral("Send to"), m_txtOutgoingIp);

    m_spnOutgoingPort = new QSpinBox(grpOsc);
    m_spnOutgoingPort->setRange(1, 65535);
    m_spnOutgoingPort->setValue(53000);
    formOsc->addRow(QStringLiteral("UDP port"), m_spnOutgoingPort);

    m_chkInvertTBar = new QCheckBox(QStringLiteral("Invert the T-bar values"), grpOsc);
    formOsc->addRow(QString(), m_chkInvertTBar);
    mainLayout->addWidget(grpOsc);

    auto *grpKeep = new QGroupBox(QStringLiteral("Connection"), page);
    grpKeep->setObjectName(QStringLiteral("section"));
    auto *keepLayout = new QVBoxLayout(grpKeep);
    m_chkAutoReconnect = new QCheckBox(QStringLiteral("Connect on startup, and reconnect if the connection is lost"), grpKeep);
    keepLayout->addWidget(m_chkAutoReconnect);
    mainLayout->addWidget(grpKeep);
    mainLayout->addStretch();
    m_stack->addWidget(page);

    // Wire signals
    connect(m_btnConnect, &QPushButton::clicked, this, &ConnectionSettingsWidget::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &ConnectionSettingsWidget::onDisconnectClicked);

    connect(m_txtIp, &QLineEdit::editingFinished, this, &ConnectionSettingsWidget::applySettings);
    connect(m_txtNickname, &QLineEdit::editingFinished, this, &ConnectionSettingsWidget::applySettings);
    connect(m_txtOutgoingIp, &QLineEdit::editingFinished, this, &ConnectionSettingsWidget::applySettings);
    connect(m_spnOutgoingPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &ConnectionSettingsWidget::applySettings);
    connect(m_chkInvertTBar, &QCheckBox::toggled, this, &ConnectionSettingsWidget::applySettings);
    connect(m_chkAutoReconnect, &QCheckBox::toggled, this, &ConnectionSettingsWidget::applySettings);

    connect(m_app, &AtemRemoteOscApp::switcherUpdated, this, [this](int idx) {
        if (idx == m_currentIndex) updateStatus();
    });

    updateUiFromConfig();
}

void ConnectionSettingsWidget::setSwitcherIndex(int index)
{
    m_currentIndex = index;
    updateUiFromConfig();
}

void ConnectionSettingsWidget::setPill(const QString &text, const char *state)
{
    m_lblPill->setText(text);
    m_lblPill->setProperty("state", QString::fromLatin1(state));
    m_lblPill->style()->unpolish(m_lblPill);
    m_lblPill->style()->polish(m_lblPill);
}

void ConnectionSettingsWidget::updateUiFromConfig()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_app->switchers().size()) {
        m_stack->setCurrentIndex(0);
        return;
    }
    m_stack->setCurrentIndex(1);
    m_updating = true;

    auto *sw = m_app->switchers()[m_currentIndex];
    m_txtIp->setText(sw->config.ip);
    m_txtNickname->setText(sw->config.nickname);
    m_txtOutgoingIp->setText(sw->config.outgoingIp);
    m_spnOutgoingPort->setValue(sw->config.outgoingPort);
    m_chkInvertTBar->setChecked(sw->config.invertTBar);
    m_chkAutoReconnect->setChecked(sw->config.autoReconnect);

    updateStatus();

    m_updating = false;
}

// Connection state only: never touches the editable fields, so a reconnection attempt in the background
// cannot overwrite an address while it is being typed.
void ConnectionSettingsWidget::updateStatus()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_app->switchers().size()) return;
    auto *sw = m_app->switchers()[m_currentIndex];
    if (!sw->backend) return;

    const QString model = sw->backend->switcherName();
    m_lblSwitcherName->setText(model.isEmpty() ? QStringLiteral("ATEM switcher") : model);

    switch (sw->backend->connectionState()) {
        case Atem::ConnectionState::Connected:
            setPill(QStringLiteral("Connected · %1").arg(sw->backend->backendName()), "ok");
            m_btnConnect->setEnabled(false);
            m_btnDisconnect->setEnabled(true);
            break;
        case Atem::ConnectionState::Connecting:
            setPill(QStringLiteral("Connecting…"), "busy");
            m_btnConnect->setEnabled(false);
            m_btnDisconnect->setEnabled(true);
            break;
        case Atem::ConnectionState::Failed:
            setPill(QStringLiteral("Connection failed"), "bad");
            m_btnConnect->setEnabled(true);
            m_btnDisconnect->setEnabled(false);
            break;
        default:
            setPill(QStringLiteral("Not connected"), "");
            m_btnConnect->setEnabled(true);
            m_btnDisconnect->setEnabled(false);
            break;
    }
}

void ConnectionSettingsWidget::applySettings()
{
    if (m_updating || m_currentIndex < 0 || m_currentIndex >= m_app->switchers().size()) return;

    SwitcherConfig cfg;
    cfg.id = m_app->switchers()[m_currentIndex]->config.id;
    cfg.ip = m_txtIp->text().trimmed();
    cfg.nickname = m_txtNickname->text().trimmed();
    cfg.outgoingIp = m_txtOutgoingIp->text().trimmed();
    cfg.outgoingPort = static_cast<quint16>(m_spnOutgoingPort->value());
    cfg.invertTBar = m_chkInvertTBar->isChecked();
    cfg.autoReconnect = m_chkAutoReconnect->isChecked();

    m_app->updateSwitcherConfig(m_currentIndex, cfg);
}

void ConnectionSettingsWidget::onConnectClicked()
{
    applySettings();
    if (m_currentIndex >= 0) m_app->connectSwitcher(m_currentIndex);
}

void ConnectionSettingsWidget::onDisconnectClicked()
{
    if (m_currentIndex >= 0) m_app->disconnectSwitcher(m_currentIndex);
}
