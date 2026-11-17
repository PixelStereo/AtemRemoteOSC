#include "ConnectionSettingsWidget.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QFrame>
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

    // Page 1: the switcher. A hero line (model, state, one big button), then two columns of cards.
    auto *page = new QWidget(m_stack);
    auto *mainLayout = new QVBoxLayout(page);
    mainLayout->setContentsMargins(0, 18, 0, 0);
    mainLayout->setSpacing(16);

    auto *hero = new QFrame(page);
    hero->setObjectName(QStringLiteral("hero"));
    auto *heroRow = new QHBoxLayout(hero);
    heroRow->setContentsMargins(20, 16, 20, 16);
    heroRow->setSpacing(16);
    auto *heroText = new QVBoxLayout;
    heroText->setSpacing(6);
    m_lblSwitcherName = new QLabel(QStringLiteral("Switcher"), hero);
    m_lblSwitcherName->setObjectName(QStringLiteral("display"));
    m_lblPill = new QLabel(hero);
    m_lblPill->setObjectName(QStringLiteral("pill"));
    heroText->addWidget(m_lblSwitcherName);
    heroText->addWidget(m_lblPill, 0, Qt::AlignLeft);
    heroRow->addLayout(heroText);
    heroRow->addStretch();
    m_btnToggle = new QPushButton(QStringLiteral("Connect"), hero);
    m_btnToggle->setProperty("role", "go");
    m_btnToggle->setObjectName(QStringLiteral("bigButton"));
    m_btnToggle->setCursor(Qt::PointingHandCursor);
    heroRow->addWidget(m_btnToggle);
    mainLayout->addWidget(hero);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(16);

    auto *grpConn = new QGroupBox(QStringLiteral("Address"), page);
    grpConn->setObjectName(QStringLiteral("section"));
    auto *connLayout = new QVBoxLayout(grpConn);
    connLayout->setSpacing(6);
    auto addField = [connLayout](const QString &caption, QWidget *w) {
        auto *l = new QLabel(caption);
        l->setObjectName(QStringLiteral("caption"));
        connLayout->addWidget(l);
        connLayout->addWidget(w);
    };
    m_txtIp = new QLineEdit(grpConn);
    m_txtIp->setPlaceholderText(QStringLiteral("e.g. 192.168.10.240"));
    addField(QStringLiteral("Switcher IP address"), m_txtIp);
    m_txtNickname = new QLineEdit(grpConn);
    m_txtNickname->setPlaceholderText(QStringLiteral("optional, tells several switchers apart"));
    addField(QStringLiteral("Name in OSC addresses"), m_txtNickname);
    m_chkAutoReconnect = new QCheckBox(QStringLiteral("Connect on startup, and reconnect if the connection is lost"), grpConn);
    connLayout->addSpacing(6);
    connLayout->addWidget(m_chkAutoReconnect);
    grpConn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    columns->addWidget(grpConn, 1);

    auto *grpOsc = new QGroupBox(QStringLiteral("OSC feedback"), page);
    grpOsc->setObjectName(QStringLiteral("section"));
    auto *oscLayout = new QVBoxLayout(grpOsc);
    oscLayout->setSpacing(6);
    auto addOsc = [oscLayout](const QString &caption, QWidget *w) {
        auto *l = new QLabel(caption);
        l->setObjectName(QStringLiteral("caption"));
        oscLayout->addWidget(l);
        oscLayout->addWidget(w);
    };
    m_txtOutgoingIp = new QLineEdit(grpOsc);
    m_txtOutgoingIp->setPlaceholderText(QStringLiteral("e.g. 127.0.0.1"));
    addOsc(QStringLiteral("Send values to"), m_txtOutgoingIp);
    m_spnOutgoingPort = new QSpinBox(grpOsc);
    m_spnOutgoingPort->setRange(1, 65535);
    m_spnOutgoingPort->setValue(53000);
    addOsc(QStringLiteral("UDP port"), m_spnOutgoingPort);
    m_chkInvertTBar = new QCheckBox(QStringLiteral("Invert the T-bar values"), grpOsc);
    oscLayout->addSpacing(6);
    oscLayout->addWidget(m_chkInvertTBar);
    grpOsc->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    columns->addWidget(grpOsc, 1);

    columns->setAlignment(Qt::AlignTop);
    mainLayout->addLayout(columns);
    mainLayout->addStretch();
    m_stack->addWidget(page);

    // Wire signals
    connect(m_btnToggle, &QPushButton::clicked, this, &ConnectionSettingsWidget::onToggleClicked);

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

    auto toggle = [this](const char *text, bool disconnect) {
        m_btnToggle->setText(QString::fromLatin1(text));
        m_btnToggle->setProperty("role", disconnect ? "stop" : "go");
        m_btnToggle->style()->unpolish(m_btnToggle);
        m_btnToggle->style()->polish(m_btnToggle);
        m_wantsDisconnect = disconnect;
    };
    switch (sw->backend->connectionState()) {
        case Atem::ConnectionState::Connected:
            setPill(QStringLiteral("Connected · %1").arg(sw->backend->backendName()), "ok");
            toggle("Disconnect", true);
            break;
        case Atem::ConnectionState::Connecting:
            setPill(QStringLiteral("Connecting…"), "busy");
            toggle("Cancel", true);
            break;
        case Atem::ConnectionState::Failed:
            setPill(QStringLiteral("Connection failed"), "bad");
            toggle("Connect", false);
            break;
        default:
            setPill(QStringLiteral("Not connected"), "");
            toggle("Connect", false);
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

void ConnectionSettingsWidget::onToggleClicked()
{
    if (m_currentIndex < 0) return;
    if (m_wantsDisconnect) {
        m_app->disconnectSwitcher(m_currentIndex);
    } else {
        applySettings();
        m_app->connectSwitcher(m_currentIndex);
    }
}
