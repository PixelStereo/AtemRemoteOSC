#pragma once

#include <QObject>
#include <QDateTime>
#include <QPair>
#include <QSettings>
#include <QTimer>
#include <QVector>
#include "../osc/OscServer.h"
#include "../osc/OscClient.h"
#include "../osc/OscAddressRegistry.h"
#include "../osc/OscDispatcher.h"
#include "../osc/OscQueryServer.h"
#include "../osc/MdnsResponder.h"
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
};

struct SwitcherInstance {
    SwitcherConfig config;
    Atem::IAtemBackend *backend = nullptr;
    Osc::Client *client = nullptr;
    QTimer *reconnectTimer = nullptr;
    bool keepTrying = false;      // a connection was established: if it is lost, reconnect until it works again
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

    // Messages logged before the log console exists (start-up connection attempts)
    const QVector<QPair<QDateTime, QString>>& logHistory() const { return m_logHistory; }

    // OSCQuery: the address space and its values over HTTP, announced on the network
    bool oscQueryEnabled() const { return m_oscQueryEnabled; }
    quint16 oscQueryPort() const { return m_oscQueryPort; }
    bool isOscQueryRunning() const;
    void setOscQuery(bool enabled, quint16 port);

    bool logDebugOsc() const { return m_logDebugOsc; }
    void setLogDebugOsc(bool enable) { m_logDebugOsc = enable; }

signals:
    void switcherListChanged();
    void switcherUpdated(int index);
    void logMessage(const QString &text);
    void oscServerStateChanged(bool listening, quint16 port);
    void oscQueryStateChanged(bool running, quint16 port);

private:
    void wireBackend(SwitcherInstance *inst);
    void restartOscQuery();

    quint16 m_incomingPort = 3333;
    bool m_logDebugOsc = false;
    bool m_oscQueryEnabled = true;
    quint16 m_oscQueryPort = 3334;
    Osc::QueryTree *m_queryTree = nullptr;
    Osc::QueryServer *m_queryServer = nullptr;
    Osc::MdnsResponder *m_mdns = nullptr;
    QVector<QPair<QDateTime, QString>> m_logHistory;
    QSettings m_settings;

    Osc::Server *m_oscServer = nullptr;
    Osc::AddressRegistry *m_registry = nullptr;
    Osc::Dispatcher *m_dispatcher = nullptr;
    QVector<SwitcherInstance*> m_switchers;
};
