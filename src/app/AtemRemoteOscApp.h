#pragma once

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVector>
#include "../osc/OscServer.h"
#include "../osc/OscClient.h"
#include "../osc/OscAddressRegistry.h"
#include "../osc/OscDispatcher.h"
#include "../backend/IAtemBackend.h"
#include "../backend/AtemBackendFactory.h"

struct SwitcherConfig {
    QString id;
    QString ip;
    QString nickname;
    QString outgoingIp = "127.0.0.1";
    quint16 outgoingPort = 53000;
    bool invertTBar = false;
    bool autoReconnect = true;
    Atem::BackendType backendType = Atem::BackendType::Auto;
};

struct SwitcherInstance {
    SwitcherConfig config;
    Atem::IAtemBackend *backend = nullptr;
    Osc::Client *client = nullptr;
    QTimer *reconnectTimer = nullptr;
    bool userDisconnected = true; // false once the user asked for a connection that should be kept alive
};

class AtemRemoteOscApp : public QObject {
    Q_OBJECT
public:
    explicit AtemRemoteOscApp(QObject *parent = nullptr);
    ~AtemRemoteOscApp() override;

    bool initialize();
    void saveSettings();
    void loadSettings();

    quint16 incomingPort() const { return m_incomingPort; }
    bool setIncomingPort(quint16 port);
    bool isOscServerListening() const;

    const QVector<SwitcherInstance*>& switchers() const { return m_switchers; }
    SwitcherInstance* addSwitcher(const SwitcherConfig &config = SwitcherConfig());
    void removeSwitcher(int index);
    void updateSwitcherConfig(int index, const SwitcherConfig &config);
    void connectSwitcher(int index);
    void disconnectSwitcher(int index);

    Osc::AddressRegistry* addressRegistry() const { return m_registry; }
    Osc::Dispatcher* dispatcher() const { return m_dispatcher; }
    Osc::Server* oscServer() const { return m_oscServer; }

    bool logDebugOsc() const { return m_logDebugOsc; }
    void setLogDebugOsc(bool enable) { m_logDebugOsc = enable; }

signals:
    void switcherListChanged();
    void switcherUpdated(int index);
    void logMessage(const QString &text);
    void oscServerStateChanged(bool listening, quint16 port);

private:
    void wireBackend(SwitcherInstance *inst);

    quint16 m_incomingPort = 3333;
    bool m_logDebugOsc = false;
    QSettings m_settings;

    Osc::Server *m_oscServer = nullptr;
    Osc::AddressRegistry *m_registry = nullptr;
    Osc::Dispatcher *m_dispatcher = nullptr;
    QVector<SwitcherInstance*> m_switchers;
};
