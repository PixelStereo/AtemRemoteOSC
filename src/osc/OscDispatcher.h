#pragma once

#include <QObject>
#include <QHash>
#include <QMap>
#include <QVector>
#include <QString>
#include <QRegularExpression>
#include "OscTypes.h"
#include "OscServer.h"
#include "OscClient.h"
#include "OscAddressRegistry.h"
#include "../backend/IAtemBackend.h"

namespace Osc {

struct SwitcherRoute {
    QString nickname;
    Atem::IAtemBackend *backend = nullptr;
    Client *client = nullptr; // OSC feedback sender
    bool invertTBar = false;

    // Last known / last sent values, keyed by parameter group. Several OSC addresses map onto one
    // backend call taking all the parameters of a group, so partial updates must be merged here.
    QHash<QString, QVector<float>> params;
    QHash<QString, QString> strings;
};

class Dispatcher : public QObject {
    Q_OBJECT
public:
    explicit Dispatcher(AddressRegistry *registry, QObject *parent = nullptr);
    ~Dispatcher() override = default;

    void addSwitcher(const QString &nickname, Atem::IAtemBackend *backend, Client *client, bool invertTBar = false);
    void removeSwitcher(Atem::IAtemBackend *backend);
    void setInvertTBar(Atem::IAtemBackend *backend, bool invert);
    void setNickname(Atem::IAtemBackend *backend, const QString &nickname);

    AddressRegistry* registry() const { return m_registry; }
    const QList<SwitcherRoute>& routes() const { return m_routes; }
    // Last value sent as feedback on a full OSC address (e.g. "/atem/me/1/program"), empty if none yet
    QVector<Value> lastFeedback(const QString &fullAddress) const { return m_lastFeedback.value(fullAddress); }
    QStringList feedbackAddresses() const { return m_lastFeedback.keys(); }

public slots:
    void dispatchMessage(const Osc::Message &message, const QHostAddress &sender, quint16 port);

signals:
    void logMessage(const QString &text);

private slots:
    // Feedback handlers from IAtemBackend
    void onConnectionStateChanged(Atem::ConnectionState state);
    void onMeStateChanged(const Atem::MeState &state);
    void onUskStateChanged(const Atem::KeyerState &state);
    void onDskStateChanged(const Atem::DskState &state);
    void onSuperSourceBoxChanged(const Atem::SuperSourceBoxState &state);
    void onAudioChannelChanged(const Atem::AudioChannelState &state);
    void onAudioMasterGainChanged(float gainDb);
    void onFairlightEqBandChanged(const Atem::FairlightEqBandState &state);
    void onFairlightDynamicsChanged(const Atem::FairlightDynamicsState &state);
    void onMultiViewChanged(const Atem::MultiViewState &state);
    void onDisplayClockChanged(const Atem::DisplayClockState &state);
    void onRecordStateChanged(const Atem::RecordState &state);
    void onStreamStateChanged(const Atem::StreamState &state);
    void onMacroStateChanged(const Atem::MacroState &state);
    void onCameraControlChanged(const Atem::CameraControlState &state);
    void onAuxSourceChanged(int auxIndex, int64_t inputId);
    void onInputChanged(const Atem::InputInfo &info);

private:
    bool handleCommand(SwitcherRoute &route, const QString &path, const Value &arg);
    bool handleMe(SwitcherRoute &route, int me, const QString &sub, const Value &arg);
    bool handleUsk(SwitcherRoute &route, int me, int key, const QString &sub, const Value &arg);
    bool handleDsk(SwitcherRoute &route, int key, const QString &sub, const Value &arg);
    bool handleSuperSource(SwitcherRoute &route, const QString &sub, const Value &arg);
    bool handleAudio(SwitcherRoute &route, const QString &sub, const Value &arg);
    bool handleMultiView(SwitcherRoute &route, int mv, const QString &sub, const Value &arg);
    bool handleClock(SwitcherRoute &route, const QString &sub, const Value &arg);
    bool handleCamera(SwitcherRoute &route, int cam, const QString &sub, const Value &arg);
    SwitcherRoute* routeFor(Atem::IAtemBackend *backend);
    void sendStatus(SwitcherRoute &route);
    void sendFeedback(Atem::IAtemBackend *backend, const QString &subAddress, const QVector<Value> &args);
    QString makeAddress(const QString &nickname, const QString &subAddress);

    AddressRegistry *m_registry = nullptr;
    QList<SwitcherRoute> m_routes;
    QHash<QString, QVector<Value>> m_lastFeedback;
};

} // namespace Osc
