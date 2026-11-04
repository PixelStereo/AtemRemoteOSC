#include "AtemRemoteOscApp.h"
#include <QUuid>

AtemRemoteOscApp::AtemRemoteOscApp(QObject *parent)
    : QObject(parent)
    , m_settings("AtemRemoteOsc", "AtemRemoteOsc")
{
}

AtemRemoteOscApp::~AtemRemoteOscApp() {
    saveSettings();
    for (auto *s : m_switchers) {
        s->userDisconnected = true;
        if (s->backend) s->backend->disconnectFromSwitcher();
        delete s->reconnectTimer;
        delete s->backend;
        delete s->client;
        delete s;
    }
    m_switchers.clear();
}

bool AtemRemoteOscApp::initialize() {
    connect(this, &AtemRemoteOscApp::logMessage, this, [this](const QString &msg) {
        m_logHistory.append({QDateTime::currentDateTime(), msg});
        if (m_logHistory.size() > 500) m_logHistory.remove(0, m_logHistory.size() - 500);
    });

    m_registry = new Osc::AddressRegistry(this);
    m_dispatcher = new Osc::Dispatcher(m_registry, this);
    m_oscServer = new Osc::Server(this);

    connect(m_dispatcher, &Osc::Dispatcher::logMessage, this, [this](const QString &msg) {
        if (m_logDebugOsc || !msg.startsWith(QLatin1String("Received OSC"))) {
            emit logMessage(msg);
        }
    });

    connect(m_oscServer, &Osc::Server::messageReceived, m_dispatcher, &Osc::Dispatcher::dispatchMessage);
    connect(m_oscServer, &Osc::Server::errorOccurred, this, [this](const QString &err) {
        emit logMessage(QStringLiteral("OSC Server Error: ") + err);
        emit oscServerStateChanged(false, m_incomingPort);
    });

    loadSettings();

    // Start listening on configured port
    setIncomingPort(m_incomingPort);

    // No switcher is preset: the list starts empty and the user adds one with its own address.
    // Switchers saved from a previous session are reconnected on launch.
    for (int i = 0; i < m_switchers.size(); ++i) {
        if (m_switchers[i]->config.autoReconnect && !m_switchers[i]->config.ip.isEmpty()) connectSwitcher(i);
    }

    return true;
}

bool AtemRemoteOscApp::setIncomingPort(quint16 port) {
    m_incomingPort = port;
    bool ok = m_oscServer->start(m_incomingPort);
    if (ok) {
        emit logMessage(QStringLiteral("OSC Server listening on UDP port %1").arg(m_incomingPort));
    } else {
        emit logMessage(QStringLiteral("Failed to bind OSC Server to UDP port %1").arg(m_incomingPort));
    }
    emit oscServerStateChanged(ok, m_incomingPort);
    return ok;
}

bool AtemRemoteOscApp::isOscServerListening() const {
    return m_oscServer && m_oscServer->isListening();
}

SwitcherInstance* AtemRemoteOscApp::addSwitcher(const SwitcherConfig &config) {
    auto *inst = new SwitcherInstance;
    inst->config = config;
    if (inst->config.id.isEmpty()) {
        inst->config.id = QUuid::createUuid().toString();
    }

    inst->backend = Atem::BackendFactory::createBackend(this);
    inst->client = new Osc::Client(this);
    inst->client->setTarget(inst->config.outgoingIp, inst->config.outgoingPort);

    inst->reconnectTimer = new QTimer(this);
    inst->reconnectTimer->setSingleShot(true);
    inst->reconnectTimer->setInterval(5000);
    connect(inst->reconnectTimer, &QTimer::timeout, this, [this, inst]() {
        const auto state = inst->backend->connectionState();
        if (!inst->userDisconnected && (state == Atem::ConnectionState::Failed || state == Atem::ConnectionState::Disconnected)) {
            inst->backend->connectTo(inst->config.ip);
        }
    });

    wireBackend(inst);

    m_dispatcher->addSwitcher(inst->config.nickname, inst->backend, inst->client, inst->config.invertTBar);
    m_switchers.append(inst);

    emit switcherListChanged();
    return inst;
}

void AtemRemoteOscApp::wireBackend(SwitcherInstance *inst) {
    connect(inst->backend, &Atem::IAtemBackend::logMessage, this, &AtemRemoteOscApp::logMessage);
    connect(inst->backend, &Atem::IAtemBackend::connectionStateChanged, this, [this, inst](Atem::ConnectionState state) {
        int idx = m_switchers.indexOf(inst);
        if (idx >= 0) emit switcherUpdated(idx);

        // Keep the link alive unless the user disconnected on purpose
        if (state == Atem::ConnectionState::Connected || state == Atem::ConnectionState::Connecting) {
            inst->reconnectTimer->stop(); // an attempt (with its own retries) is running or done
        } else if (!inst->userDisconnected && inst->config.autoReconnect && !inst->backend->permanentFailure()) {
            inst->reconnectTimer->start();
        }
    });
    connect(inst->backend, &Atem::IAtemBackend::switcherNameChanged, this, [this, inst]() {
        int idx = m_switchers.indexOf(inst);
        if (idx >= 0) emit switcherUpdated(idx);
    });
}

void AtemRemoteOscApp::removeSwitcher(int index) {
    if (index >= 0 && index < m_switchers.size()) {
        auto *inst = m_switchers.takeAt(index);
        inst->userDisconnected = true;
        m_dispatcher->removeSwitcher(inst->backend);
        inst->backend->disconnectFromSwitcher();
        delete inst->reconnectTimer;
        delete inst->backend;
        delete inst->client;
        delete inst;
        emit switcherListChanged();
    }
}

void AtemRemoteOscApp::updateSwitcherConfig(int index, const SwitcherConfig &config) {
    if (index >= 0 && index < m_switchers.size()) {
        auto *inst = m_switchers[index];
        inst->config = config;

        inst->client->setTarget(config.outgoingIp, config.outgoingPort);

            m_dispatcher->setInvertTBar(inst->backend, config.invertTBar);
            m_dispatcher->setNickname(inst->backend, config.nickname);

        emit switcherUpdated(index);
    }
}

void AtemRemoteOscApp::connectSwitcher(int index) {
    if (index >= 0 && index < m_switchers.size()) {
        auto *inst = m_switchers[index];
        if (inst->config.ip.trimmed().isEmpty()) {
            emit logMessage(QStringLiteral("Enter the IP address of the switcher before connecting."));
            return;
        }
        inst->userDisconnected = false;
        inst->backend->connectTo(inst->config.ip);
    }
}

void AtemRemoteOscApp::disconnectSwitcher(int index) {
    if (index >= 0 && index < m_switchers.size()) {
        auto *inst = m_switchers[index];
        inst->userDisconnected = true;
        inst->reconnectTimer->stop();
        inst->backend->disconnectFromSwitcher();
    }
}

void AtemRemoteOscApp::loadSettings() {
    m_incomingPort = static_cast<quint16>(m_settings.value("incomingPort", 3333).toUInt());
    m_logDebugOsc = m_settings.value("logDebugOsc", false).toBool();

    int size = m_settings.beginReadArray("switchers");
    for (int i = 0; i < size; ++i) {
        m_settings.setArrayIndex(i);
        SwitcherConfig cfg;
        cfg.id = m_settings.value("id").toString();
        cfg.ip = m_settings.value("ip").toString();
        cfg.nickname = m_settings.value("nickname").toString();
        cfg.outgoingIp = m_settings.value("outgoingIp", "127.0.0.1").toString();
        cfg.outgoingPort = static_cast<quint16>(m_settings.value("outgoingPort", 53000).toUInt());
        cfg.invertTBar = m_settings.value("invertTBar", false).toBool();
        cfg.autoReconnect = m_settings.value("autoReconnect", true).toBool();
        addSwitcher(cfg);
    }
    m_settings.endArray();
}

void AtemRemoteOscApp::saveSettings() {
    m_settings.setValue("incomingPort", m_incomingPort);
    m_settings.setValue("logDebugOsc", m_logDebugOsc);

    m_settings.beginWriteArray("switchers", m_switchers.size());
    for (int i = 0; i < m_switchers.size(); ++i) {
        m_settings.setArrayIndex(i);
        const auto &cfg = m_switchers[i]->config;
        m_settings.setValue("id", cfg.id);
        m_settings.setValue("ip", cfg.ip);
        m_settings.setValue("nickname", cfg.nickname);
        m_settings.setValue("outgoingIp", cfg.outgoingIp);
        m_settings.setValue("outgoingPort", cfg.outgoingPort);
        m_settings.setValue("invertTBar", cfg.invertTBar);
        m_settings.setValue("autoReconnect", cfg.autoReconnect);
    }
    m_settings.endArray();
}
