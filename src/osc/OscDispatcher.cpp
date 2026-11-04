#include "OscDispatcher.h"
#include <QDebug>
#include <QRegularExpression>

namespace Osc {

namespace {

// Returns the cached parameter group, creating it with `defaults` the first time.
QVector<float>& group(SwitcherRoute &r, const QString &key, std::initializer_list<float> defaults) {
    auto it = r.params.find(key);
    if (it == r.params.end()) {
        it = r.params.insert(key, QVector<float>(defaults));
    }
    return it.value();
}

Atem::TransitionStyle parseTransitionStyle(const QString &s) {
    const QString v = s.toLower();
    if (v == QLatin1String("dip")) return Atem::TransitionStyle::Dip;
    if (v == QLatin1String("wipe")) return Atem::TransitionStyle::Wipe;
    if (v == QLatin1String("dve")) return Atem::TransitionStyle::DVE;
    if (v == QLatin1String("sting") || v == QLatin1String("stinger")) return Atem::TransitionStyle::Stinger;
    return Atem::TransitionStyle::Mix;
}

Atem::KeyType parseKeyType(const QString &s) {
    const QString v = s.toLower();
    if (v == QLatin1String("chroma")) return Atem::KeyType::Chroma;
    if (v == QLatin1String("pattern")) return Atem::KeyType::Pattern;
    if (v == QLatin1String("dve")) return Atem::KeyType::DVE;
    return Atem::KeyType::Luma;
}

Atem::MultiViewLayout parseMultiViewLayout(const QString &s) {
    const QString v = s.toLower();
    if (v == QLatin1String("top")) return Atem::MultiViewLayout::Top;
    if (v == QLatin1String("bottom")) return Atem::MultiViewLayout::Bottom;
    if (v == QLatin1String("left")) return Atem::MultiViewLayout::Left;
    if (v == QLatin1String("right")) return Atem::MultiViewLayout::Right;
    if (v == QLatin1String("quadrant")) return Atem::MultiViewLayout::Quadrant;
    if (v == QLatin1String("1+5")) return Atem::MultiViewLayout::OnePlusFive;
    if (v == QLatin1String("2+6")) return Atem::MultiViewLayout::TwoPlusSix;
    if (v == QLatin1String("1+7")) return Atem::MultiViewLayout::OnePlusSeven;
    if (v == QLatin1String("2+8")) return Atem::MultiViewLayout::TwoPlusEight;
    if (v == QLatin1String("1+9")) return Atem::MultiViewLayout::OnePlusNine;
    return Atem::MultiViewLayout::Default;
}

Atem::ClockMode parseClockMode(const QString &s) {
    const QString v = s.toLower();
    if (v == QLatin1String("countdown")) return Atem::ClockMode::CountDown;
    if (v == QLatin1String("countup")) return Atem::ClockMode::CountUp;
    return Atem::ClockMode::TimeOfDay;
}

Atem::AudioMixOption parseMixOption(const QString &s) {
    const QString v = s.toLower();
    if (v == QLatin1String("on")) return Atem::AudioMixOption::On;
    if (v == QLatin1String("afv")) return Atem::AudioMixOption::AudioFollowVideo;
    return Atem::AudioMixOption::Off;
}

QString connectionLedAddress(bool green) {
    return green ? QStringLiteral("/led/green") : QStringLiteral("/led/red");
}

} // namespace

Dispatcher::Dispatcher(AddressRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
}

void Dispatcher::addSwitcher(const QString &nickname, Atem::IAtemBackend *backend, Client *client, bool invertTBar) {
    if (!backend) return;
    SwitcherRoute route;
    route.nickname = nickname;
    route.backend = backend;
    route.client = client;
    route.invertTBar = invertTBar;
    m_routes.append(route);

    connect(backend, &Atem::IAtemBackend::connectionStateChanged, this, &Dispatcher::onConnectionStateChanged);
    connect(backend, &Atem::IAtemBackend::meStateChanged, this, &Dispatcher::onMeStateChanged);
    connect(backend, &Atem::IAtemBackend::uskStateChanged, this, &Dispatcher::onUskStateChanged);
    connect(backend, &Atem::IAtemBackend::dskStateChanged, this, &Dispatcher::onDskStateChanged);
    connect(backend, &Atem::IAtemBackend::superSourceBoxChanged, this, &Dispatcher::onSuperSourceBoxChanged);
    connect(backend, &Atem::IAtemBackend::audioChannelChanged, this, &Dispatcher::onAudioChannelChanged);
    connect(backend, &Atem::IAtemBackend::audioMasterGainChanged, this, &Dispatcher::onAudioMasterGainChanged);
    connect(backend, &Atem::IAtemBackend::fairlightEqBandChanged, this, &Dispatcher::onFairlightEqBandChanged);
    connect(backend, &Atem::IAtemBackend::fairlightDynamicsChanged, this, &Dispatcher::onFairlightDynamicsChanged);
    connect(backend, &Atem::IAtemBackend::multiViewChanged, this, &Dispatcher::onMultiViewChanged);
    connect(backend, &Atem::IAtemBackend::displayClockChanged, this, &Dispatcher::onDisplayClockChanged);
    connect(backend, &Atem::IAtemBackend::recordStateChanged, this, &Dispatcher::onRecordStateChanged);
    connect(backend, &Atem::IAtemBackend::streamStateChanged, this, &Dispatcher::onStreamStateChanged);
    connect(backend, &Atem::IAtemBackend::macroStateChanged, this, &Dispatcher::onMacroStateChanged);
    connect(backend, &Atem::IAtemBackend::cameraControlChanged, this, &Dispatcher::onCameraControlChanged);
    connect(backend, &Atem::IAtemBackend::auxSourceChanged, this, &Dispatcher::onAuxSourceChanged);
    connect(backend, &Atem::IAtemBackend::inputChanged, this, &Dispatcher::onInputChanged);
}

void Dispatcher::removeSwitcher(Atem::IAtemBackend *backend) {
    for (int i = 0; i < m_routes.size(); ++i) {
        if (m_routes[i].backend == backend) {
            backend->disconnect(this);
            m_routes.removeAt(i);
            break;
        }
    }
}

void Dispatcher::setInvertTBar(Atem::IAtemBackend *backend, bool invert) {
    if (auto *r = routeFor(backend)) r->invertTBar = invert;
}

void Dispatcher::setNickname(Atem::IAtemBackend *backend, const QString &nickname) {
    if (auto *r = routeFor(backend)) r->nickname = nickname;
}

SwitcherRoute* Dispatcher::routeFor(Atem::IAtemBackend *backend) {
    for (auto &r : m_routes) {
        if (r.backend == backend) return &r;
    }
    return nullptr;
}

QString Dispatcher::makeAddress(const QString &nickname, const QString &subAddress) {
    if (nickname.isEmpty()) {
        return QStringLiteral("/atem") + subAddress;
    }
    return QStringLiteral("/atem/") + nickname + subAddress;
}

void Dispatcher::sendFeedback(Atem::IAtemBackend *backend, const QString &subAddress, const QVector<Value> &args) {
    for (const auto &r : m_routes) {
        if (r.backend != backend) continue;
        const QString fullAddr = makeAddress(r.nickname, subAddress);
        m_lastFeedback.insert(fullAddr, args); // also served by OSCQuery
        if (r.client && r.client->isValidTarget()) {
            r.client->sendMessage(fullAddr, args);
        }
    }
}

void Dispatcher::sendStatus(SwitcherRoute &route) {
    route.backend->requestStatus();
    const bool connected = route.backend->connectionState() == Atem::ConnectionState::Connected;
    sendFeedback(route.backend, connectionLedAddress(true), { Value(connected ? 1.0f : 0.0f) });
    sendFeedback(route.backend, connectionLedAddress(false), { Value(connected ? 0.0f : 1.0f) });
    sendFeedback(route.backend, QStringLiteral("/macros/max-number"), { Value(static_cast<int32_t>(route.backend->maxMacros())) });
}

void Dispatcher::dispatchMessage(const Osc::Message &message, const QHostAddress &sender, quint16 port) {
    const QString addr = message.address();
    if (addr != QLatin1String("/atem") && !addr.startsWith(QLatin1String("/atem/"))) {
        return; // Ignore non-atem messages
    }

    emit logMessage(QStringLiteral("Received OSC: %1 from %2:%3")
                        .arg(message.printString(), sender.toString()).arg(port));

    // Strip "/atem"
    QString remaining = addr.mid(5);
    if (remaining.startsWith('/')) remaining = remaining.mid(1);

    // Identify switcher route: "/atem/<nickname>/..." or the first switcher without nickname
    SwitcherRoute *targetRoute = nullptr;
    QString cmdPath = remaining;

    for (auto &r : m_routes) {
        if (!r.nickname.isEmpty() && remaining.startsWith(r.nickname + QLatin1Char('/'))) {
            targetRoute = &r;
            cmdPath = remaining.mid(r.nickname.length() + 1);
            break;
        }
    }

    if (!targetRoute) {
        for (auto &r : m_routes) {
            if (r.nickname.isEmpty()) {
                targetRoute = &r;
                break;
            }
        }
        if (!targetRoute && !m_routes.isEmpty()) {
            targetRoute = &m_routes.first();
        }
    }

    if (!targetRoute || !targetRoute->backend) {
        emit logMessage(QStringLiteral("Error: No switcher available to handle address %1").arg(addr));
        return;
    }

    if (!cmdPath.startsWith('/')) {
        cmdPath.prepend('/');
    }
    while (cmdPath.size() > 1 && cmdPath.endsWith('/')) cmdPath.chop(1);

    const Value arg0 = message.arg(0);
    if (handleCommand(*targetRoute, cmdPath, arg0)) {
        return;
    }

    // Value-in-address form (TouchOSC style): /atem/program/5, /atem/usk/1/type/luma, ...
    // TouchOSC buttons send 1.0 on press and 0.0 on release together with such an address.
    const bool numericTrigger = message.argumentCount() == 1 && (arg0.isInt() || arg0.isFloat() || arg0.isBool());
    if (message.argumentCount() == 0 || numericTrigger) {
        if (numericTrigger && arg0.toFloat() == 0.0f) {
            return; // button release
        }
        const int slash = cmdPath.lastIndexOf('/');
        if (slash > 0) {
            const QString tail = cmdPath.mid(slash + 1);
            bool isNumber = false;
            const int number = tail.toInt(&isNumber);
            const Value tailValue = isNumber ? Value(static_cast<int32_t>(number)) : Value(tail);
            if (handleCommand(*targetRoute, cmdPath.left(slash), tailValue)) {
                return;
            }
        }
    }

    emit logMessage(QStringLiteral("Unknown or unsupported OSC address: %1").arg(addr));
}

bool Dispatcher::handleCommand(SwitcherRoute &r, const QString &path, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;

    if (path == QLatin1String("/send-status")) {
        sendStatus(r);
        return true;
    }

    // Default M/E (1) aliases: /program, /preview, /transition/..., /usk/...
    static const QRegularExpression rxMeAlias("^/(program|preview|transition|usk)(/.*)?$");
    auto matchAlias = rxMeAlias.match(path);
    if (matchAlias.hasMatch()) {
        return handleMe(r, 1, path.mid(1), arg);
    }

    static const QRegularExpression rxMe("^/me/(\\d+)/(.+)$");
    auto matchMe = rxMe.match(path);
    if (matchMe.hasMatch()) {
        return handleMe(r, matchMe.captured(1).toInt(), matchMe.captured(2), arg);
    }

    static const QRegularExpression rxDsk("^/dsk/(\\d+)/(.+)$");
    auto matchDsk = rxDsk.match(path);
    if (matchDsk.hasMatch()) {
        return handleDsk(r, matchDsk.captured(1).toInt(), matchDsk.captured(2), arg);
    }

    if (path.startsWith(QLatin1String("/supersource/"))) {
        return handleSuperSource(r, path.mid(13), arg);
    }

    if (path.startsWith(QLatin1String("/audio/"))) {
        return handleAudio(r, path.mid(7), arg);
    }

    static const QRegularExpression rxMv("^/multiview/(\\d+)/(.+)$");
    auto matchMv = rxMv.match(path);
    if (matchMv.hasMatch()) {
        return handleMultiView(r, matchMv.captured(1).toInt(), matchMv.captured(2), arg);
    }

    if (path.startsWith(QLatin1String("/clock/"))) {
        return handleClock(r, path.mid(7), arg);
    }

    static const QRegularExpression rxCam("^/camera/(\\d+)/(.+)$");
    auto matchCam = rxCam.match(path);
    if (matchCam.hasMatch()) {
        return handleCamera(r, matchCam.captured(1).toInt(), matchCam.captured(2), arg);
    }

    // Recording & streaming
    if (path == QLatin1String("/recording/start")) { sw->startRecording(); return true; }
    if (path == QLatin1String("/recording/stop")) { sw->stopRecording(); return true; }
    if (path == QLatin1String("/recording/switch-disk")) { sw->switchRecordingDisk(); return true; }
    if (path == QLatin1String("/recording/filename")) { sw->setRecordingFilename(arg.toString()); return true; }
    if (path == QLatin1String("/recording/iso")) { sw->setRecordAllInputsIso(arg.toBool()); return true; }
    if (path == QLatin1String("/stream/start")) { sw->startStreaming(); return true; }
    if (path == QLatin1String("/stream/stop")) { sw->stopStreaming(); return true; }
    if (path == QLatin1String("/stream/service") || path == QLatin1String("/stream/url") || path == QLatin1String("/stream/key")) {
        r.strings.insert(path, arg.toString());
        sw->setStreamingService(r.strings.value(QStringLiteral("/stream/service")),
                                r.strings.value(QStringLiteral("/stream/url")),
                                r.strings.value(QStringLiteral("/stream/key")));
        return true;
    }

    // Macros
    if (path == QLatin1String("/macros/stop")) { sw->stopMacro(); return true; }
    static const QRegularExpression rxMac("^/macros/(\\d+)/(run|loop)$");
    auto matchMac = rxMac.match(path);
    if (matchMac.hasMatch()) {
        if (matchMac.captured(2) == QLatin1String("run")) {
            sw->runMacro(matchMac.captured(1).toUInt());
        } else {
            sw->setMacroLoop(arg.toBool());
        }
        return true;
    }

    // HyperDeck
    static const QRegularExpression rxHd("^/hyperdeck/(\\d+)/(.+)$");
    auto matchHd = rxHd.match(path);
    if (matchHd.hasMatch()) {
        const int deck = matchHd.captured(1).toInt();
        const QString sub = matchHd.captured(2);
        if (sub == QLatin1String("play")) sw->hyperDeckPlay(deck);
        else if (sub == QLatin1String("stop")) sw->hyperDeckStop(deck);
        else if (sub == QLatin1String("record")) sw->hyperDeckRecord(deck);
        else if (sub == QLatin1String("shuttle")) sw->hyperDeckShuttle(deck, arg.toInt());
        else if (sub == QLatin1String("jog")) sw->hyperDeckJog(deck, arg.toInt());
        else if (sub == QLatin1String("clip")) sw->hyperDeckSelectClip(deck, arg.toInt());
        else if (sub == QLatin1String("clip-time")) sw->hyperDeckSetClipTime(deck, arg.toString());
        else if (sub == QLatin1String("timeline-time")) sw->hyperDeckSetTimelineTime(deck, arg.toString());
        else if (sub == QLatin1String("single-clip")) sw->hyperDeckSetSingleClip(deck, arg.toBool());
        else if (sub == QLatin1String("loop")) sw->hyperDeckSetLooped(deck, arg.toBool());
        else return false;
        return true;
    }

    // Media players
    if (path == QLatin1String("/mplayer/capture")) { sw->captureStill(arg.toInt()); return true; }
    static const QRegularExpression rxMp("^/mplayer/(\\d+)/(clip|still)$");
    auto matchMp = rxMp.match(path);
    if (matchMp.hasMatch()) {
        const int player = matchMp.captured(1).toInt();
        if (matchMp.captured(2) == QLatin1String("clip")) sw->selectMediaPlayerClip(player, arg.toInt());
        else sw->selectMediaPlayerStill(player, arg.toInt());
        return true;
    }

    // Aux outputs
    static const QRegularExpression rxAux("^/aux/(\\d+)(?:/(converted-videomode))?$");
    auto matchAux = rxAux.match(path);
    if (matchAux.hasMatch()) {
        const int aux = matchAux.captured(1).toInt();
        if (matchAux.captured(2).isEmpty()) sw->setAuxSource(aux, arg.toInt());
        else sw->setConvertedVideoMode(aux, static_cast<uint32_t>(arg.toInt()));
        return true;
    }

    return false;
}

bool Dispatcher::handleMe(SwitcherRoute &r, int me, const QString &sub, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;

    if (sub == QLatin1String("send-status")) { sw->requestStatus(); return true; }
    if (sub == QLatin1String("program")) { sw->setProgramInput(me, arg.toInt()); return true; }
    if (sub == QLatin1String("preview")) { sw->setPreviewInput(me, arg.toInt()); return true; }

    if (sub == QLatin1String("transition/cut")) { sw->performCut(me); return true; }
    if (sub == QLatin1String("transition/auto")) { sw->performAutoTransition(me); return true; }
    if (sub == QLatin1String("transition/ftb")) { sw->performFadeToBlack(me); return true; }
    if (sub == QLatin1String("transition/bar") || sub == QLatin1String("transition/position")) {
        float pos = qBound(0.0f, arg.toFloat(), 1.0f);
        if (r.invertTBar) pos = 1.0f - pos;
        sw->setTransitionPosition(me, pos);
        return true;
    }
    if (sub == QLatin1String("transition/bar/reset")) {
        sw->setTransitionPosition(me, 0.0f);
        return true;
    }
    if (sub == QLatin1String("transition/preview")) { sw->setPreviewTransition(me, arg.toBool()); return true; }
    if (sub == QLatin1String("transition/type")) {
        sw->setTransitionStyle(me, parseTransitionStyle(arg.toString()));
        return true;
    }
    if (sub == QLatin1String("transition/rate")) {
        sw->setTransitionRate(me, sw->meBlocks().value(me).transitionStyle, arg.toFloat());
        return true;
    }
    if (sub == QLatin1String("transition/mix/rate")) { sw->setTransitionRate(me, Atem::TransitionStyle::Mix, arg.toFloat()); return true; }
    if (sub == QLatin1String("transition/dip/rate")) { sw->setTransitionRate(me, Atem::TransitionStyle::Dip, arg.toFloat()); return true; }
    if (sub == QLatin1String("transition/wipe/rate")) { sw->setTransitionRate(me, Atem::TransitionStyle::Wipe, arg.toFloat()); return true; }
    if (sub == QLatin1String("transition/dve/rate")) { sw->setTransitionRate(me, Atem::TransitionStyle::DVE, arg.toFloat()); return true; }

    static const QRegularExpression rxUsk("^usk/(\\d+)/(.+)$");
    auto matchUsk = rxUsk.match(sub);
    if (matchUsk.hasMatch()) {
        return handleUsk(r, me, matchUsk.captured(1).toInt(), matchUsk.captured(2), arg);
    }
    return false;
}

bool Dispatcher::handleUsk(SwitcherRoute &r, int me, int key, const QString &sub, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;
    const QString k = QStringLiteral("usk/%1/%2/").arg(me).arg(key);
    QVector<float> &state = group(r, k + QStringLiteral("state"), {0.0f, 0.0f}); // onAir, tie

    if (sub == QLatin1String("tie")) {
        state[1] = arg.toBool();
        sw->setUskTie(me, key, arg.toBool());
    } else if (sub == QLatin1String("tie/toggle")) {
        state[1] = state[1] == 0.0f;
        sw->setUskTie(me, key, state[1] != 0.0f);
    } else if (sub == QLatin1String("tie/set-next")) {
        // The argument is the desired on-air state after the next transition
        const bool tie = (state[0] != 0.0f) != arg.toBool();
        state[1] = tie;
        sw->setUskTie(me, key, tie);
    } else if (sub == QLatin1String("on-air")) {
        state[0] = arg.toBool();
        sw->setUskOnAir(me, key, arg.toBool());
    } else if (sub == QLatin1String("on-air/toggle")) {
        state[0] = state[0] == 0.0f;
        sw->setUskOnAir(me, key, state[0] != 0.0f);
    } else if (sub == QLatin1String("type")) {
        sw->setUskType(me, key, parseKeyType(arg.toString()));
    } else if (sub == QLatin1String("source/fill")) {
        sw->setUskFillSource(me, key, arg.toInt());
    } else if (sub == QLatin1String("source/cut")) {
        sw->setUskCutSource(me, key, arg.toInt());
    } else if (sub.startsWith(QLatin1String("luma/"))) {
        auto &g = group(r, k + QStringLiteral("luma"), {0.0f, 0.5f, 1.0f, 0.0f}); // preMult, clip, gain, inverse
        const QString p = sub.mid(5);
        if (p == QLatin1String("clip")) g[1] = arg.toFloat();
        else if (p == QLatin1String("gain")) g[2] = arg.toFloat();
        else if (p == QLatin1String("pre-multiplied")) g[0] = arg.toBool();
        else if (p == QLatin1String("inverse")) g[3] = arg.toBool();
        else return false;
        sw->setUskLumaParams(me, key, g[0] != 0.0f, g[1], g[2], g[3] != 0.0f);
    } else if (sub.startsWith(QLatin1String("chroma/sample-"))) {
        auto &g = group(r, k + QStringLiteral("chroma-sample"), {0.5f, 0.5f});
        const QString p = sub.mid(14);
        if (p == QLatin1String("x")) g[0] = arg.toFloat();
        else if (p == QLatin1String("y")) g[1] = arg.toFloat();
        else return false;
        sw->setUskAdvancedChromaSample(me, key, g[0], g[1]);
    } else if (sub.startsWith(QLatin1String("chroma/"))) {
        auto &g = group(r, k + QStringLiteral("chroma"), {0.0f, 1.0f, 1.0f, 0.0f, 0.0f}); // hue, gain, ySuppress, lift, narrow
        const QString p = sub.mid(7);
        if (p == QLatin1String("hue")) g[0] = arg.toFloat();
        else if (p == QLatin1String("gain")) g[1] = arg.toFloat();
        else if (p == QLatin1String("y-suppress")) g[2] = arg.toFloat();
        else if (p == QLatin1String("lift")) g[3] = arg.toFloat();
        else if (p == QLatin1String("narrow")) g[4] = arg.toBool();
        else return false;
        sw->setUskChromaParams(me, key, g[0], g[1], g[2], g[3], g[4] != 0.0f);
    } else if (sub == QLatin1String("fly/enable") || sub == QLatin1String("fly/enabled")) {
        sw->setUskFlyEnabled(me, key, arg.toBool());
    } else if (sub == QLatin1String("fly/position-x") || sub == QLatin1String("fly/position-y")) {
        auto &g = group(r, k + QStringLiteral("fly-pos"), {0.0f, 0.0f});
        g[sub.endsWith(QLatin1Char('x')) ? 0 : 1] = arg.toFloat();
        sw->setUskFlyPosition(me, key, g[0], g[1]);
    } else if (sub == QLatin1String("fly/size-x") || sub == QLatin1String("fly/size-y")) {
        auto &g = group(r, k + QStringLiteral("fly-size"), {1.0f, 1.0f});
        g[sub.endsWith(QLatin1Char('x')) ? 0 : 1] = arg.toFloat();
        sw->setUskFlySize(me, key, g[0], g[1]);
    } else if (sub == QLatin1String("fly/rotation")) {
        sw->setUskFlyRotation(me, key, arg.toFloat());
    } else if (sub == QLatin1String("fly/reset")) {
        sw->resetUskFly(me, key);
        group(r, k + QStringLiteral("fly-pos"), {0.0f, 0.0f}).fill(0.0f);
        group(r, k + QStringLiteral("fly-size"), {1.0f, 1.0f}).fill(1.0f);
    } else if (sub == QLatin1String("fly/clear")) {
        // Full screen, no rotation
        group(r, k + QStringLiteral("fly-pos"), {0.0f, 0.0f}).fill(0.0f);
        group(r, k + QStringLiteral("fly-size"), {1.0f, 1.0f}).fill(1.0f);
        sw->setUskFlyPosition(me, key, 0.0f, 0.0f);
        sw->setUskFlySize(me, key, 1.0f, 1.0f);
        sw->setUskFlyRotation(me, key, 0.0f);
    } else if (sub.startsWith(QLatin1String("mask/"))) {
        auto &g = group(r, k + QStringLiteral("mask"), {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}); // enabled, top, bottom, left, right
        const QString p = sub.mid(5);
        if (p == QLatin1String("reset")) {
            sw->resetUskMask(me, key);
            g.fill(0.0f);
            return true;
        }
        if (p == QLatin1String("enabled")) g[0] = arg.toBool();
        else if (p == QLatin1String("top")) g[1] = arg.toFloat();
        else if (p == QLatin1String("bottom")) g[2] = arg.toFloat();
        else if (p == QLatin1String("left")) g[3] = arg.toFloat();
        else if (p == QLatin1String("right")) g[4] = arg.toFloat();
        else return false;
        sw->setUskMask(me, key, g[0] != 0.0f, g[1], g[2], g[3], g[4]);
    } else if (sub.startsWith(QLatin1String("dve/")) || sub == QLatin1String("fly/rate")) {
        emit logMessage(QStringLiteral("USK %1 address /%2 is not implemented by AtemRemoteOsc yet").arg(key).arg(sub));
    } else {
        return false;
    }
    return true;
}

bool Dispatcher::handleDsk(SwitcherRoute &r, int key, const QString &sub, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;
    const QString k = QStringLiteral("dsk/%1/").arg(key);
    QVector<float> &state = group(r, k + QStringLiteral("state"), {0.0f, 0.0f}); // onAir, tie

    if (sub == QLatin1String("tie")) {
        state[1] = arg.toBool();
        sw->setDskTie(key, arg.toBool());
    } else if (sub == QLatin1String("tie/toggle")) {
        state[1] = state[1] == 0.0f;
        sw->setDskTie(key, state[1] != 0.0f);
    } else if (sub == QLatin1String("tie/set-next")) {
        const bool tie = (state[0] != 0.0f) != arg.toBool();
        state[1] = tie;
        sw->setDskTie(key, tie);
    } else if (sub == QLatin1String("on-air")) {
        state[0] = arg.toBool();
        sw->setDskOnAir(key, arg.toBool());
    } else if (sub == QLatin1String("on-air/toggle")) {
        state[0] = state[0] == 0.0f;
        sw->setDskOnAir(key, state[0] != 0.0f);
    } else if (sub == QLatin1String("on-air/auto")) {
        sw->performDskAutoTransition(key);
    } else if (sub == QLatin1String("rate")) {
        sw->setDskRate(key, arg.toInt());
    } else if (sub == QLatin1String("source/fill")) {
        sw->setDskFillSource(key, arg.toInt());
    } else if (sub == QLatin1String("source/cut")) {
        sw->setDskCutSource(key, arg.toInt());
    } else if (sub == QLatin1String("clip") || sub == QLatin1String("gain")
               || sub == QLatin1String("pre-multiplied") || sub == QLatin1String("inverse")) {
        auto &g = group(r, k + QStringLiteral("luma"), {0.0f, 0.5f, 1.0f, 0.0f}); // preMult, clip, gain, inverse
        if (sub == QLatin1String("clip")) g[1] = arg.toFloat();
        else if (sub == QLatin1String("gain")) g[2] = arg.toFloat();
        else if (sub == QLatin1String("pre-multiplied")) g[0] = arg.toBool();
        else g[3] = arg.toBool();
        sw->setDskLumaParams(key, g[0] != 0.0f, g[1], g[2], g[3] != 0.0f);
    } else if (sub.startsWith(QLatin1String("mask/"))) {
        auto &g = group(r, k + QStringLiteral("mask"), {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}); // enabled, top, bottom, left, right
        const QString p = sub.mid(5);
        if (p == QLatin1String("reset")) {
            sw->resetDskMask(key);
            g.fill(0.0f);
            return true;
        }
        if (p == QLatin1String("enabled")) g[0] = arg.toBool();
        else if (p == QLatin1String("top")) g[1] = arg.toFloat();
        else if (p == QLatin1String("bottom")) g[2] = arg.toFloat();
        else if (p == QLatin1String("left")) g[3] = arg.toFloat();
        else if (p == QLatin1String("right")) g[4] = arg.toFloat();
        else return false;
        sw->setDskMask(key, g[0] != 0.0f, g[1], g[2], g[3], g[4]);
    } else {
        return false;
    }
    return true;
}

bool Dispatcher::handleSuperSource(SwitcherRoute &r, const QString &sub, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;

    if (sub.startsWith(QLatin1String("border/"))) {
        // enabled, widthIn, widthOut, softnessIn, softnessOut, hue, saturation, luma
        auto &g = group(r, QStringLiteral("ssrc/border"), {0.0f, 0.05f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f});
        const QString p = sub.mid(7);
        if (p == QLatin1String("enabled")) g[0] = arg.toBool();
        else if (p == QLatin1String("width-in")) g[1] = arg.toFloat();
        else if (p == QLatin1String("width-out")) g[2] = arg.toFloat();
        else if (p == QLatin1String("softness-in")) g[3] = arg.toFloat();
        else if (p == QLatin1String("softness-out")) g[4] = arg.toFloat();
        else if (p == QLatin1String("hue")) g[5] = arg.toFloat();
        else if (p == QLatin1String("saturation")) g[6] = arg.toFloat();
        else if (p == QLatin1String("luma")) g[7] = arg.toFloat();
        else return false;
        sw->setSuperSourceBorder(g[0] != 0.0f, g[1], g[2], g[3], g[4], g[5], g[6], g[7]);
        return true;
    }

    // "box/<n>/..." and the legacy "<n>/..."
    static const QRegularExpression rxBox("^(?:box/)?(\\d+)/(.+)$");
    auto match = rxBox.match(sub);
    if (!match.hasMatch()) return false;

    const int box = match.captured(1).toInt();
    const QString p = match.captured(2);
    const QString k = QStringLiteral("ssrc/%1/").arg(box);

    if (p == QLatin1String("enabled")) {
        sw->setSuperSourceBoxEnabled(box, arg.toBool());
    } else if (p == QLatin1String("source")) {
        sw->setSuperSourceBoxSource(box, arg.toInt());
    } else if (p == QLatin1String("x") || p == QLatin1String("y")) {
        auto &g = group(r, k + QStringLiteral("pos"), {0.0f, 0.0f});
        g[p == QLatin1String("x") ? 0 : 1] = arg.toFloat();
        sw->setSuperSourceBoxPosition(box, g[0], g[1]);
    } else if (p == QLatin1String("size")) {
        sw->setSuperSourceBoxSize(box, arg.toFloat());
    } else if (p == QLatin1String("crop-reset")) {
        group(r, k + QStringLiteral("crop"), {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}).fill(0.0f);
        sw->resetSuperSourceBoxCrop(box);
    } else if (p == QLatin1String("cropped") || p.startsWith(QLatin1String("crop-"))) {
        auto &g = group(r, k + QStringLiteral("crop"), {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}); // cropped, top, bottom, left, right
        if (p == QLatin1String("cropped")) g[0] = arg.toBool();
        else if (p == QLatin1String("crop-top")) g[1] = arg.toFloat();
        else if (p == QLatin1String("crop-bottom")) g[2] = arg.toFloat();
        else if (p == QLatin1String("crop-left")) g[3] = arg.toFloat();
        else if (p == QLatin1String("crop-right")) g[4] = arg.toFloat();
        else return false;
        sw->setSuperSourceBoxCrop(box, g[0] != 0.0f, g[1], g[2], g[3], g[4]);
    } else {
        return false;
    }
    return true;
}

bool Dispatcher::handleAudio(SwitcherRoute &r, const QString &sub, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;

    if (sub == QLatin1String("output/gain")) {
        sw->setAudioMasterGain(arg.toFloat());
        return true;
    }

    static const QRegularExpression rxIn("^input/(\\d+)/(.+)$");
    auto match = rxIn.match(sub);
    if (!match.hasMatch()) return false;

    const int64_t id = match.captured(1).toLongLong();
    const QString p = match.captured(2);
    const QString k = QStringLiteral("aud/%1/").arg(id);

    if (p == QLatin1String("gain")) {
        sw->setAudioInputGain(id, arg.toFloat());
    } else if (p == QLatin1String("balance") || p == QLatin1String("pan")) {
        sw->setAudioInputPan(id, arg.toFloat());
    } else if (p == QLatin1String("mix")) {
        sw->setAudioInputMixOption(id, parseMixOption(arg.toString()));
    } else if (p == QLatin1String("solo")) {
        sw->setAudioSolo(id, arg.toBool());
    } else if (p == QLatin1String("preamp") || p == QLatin1String("mic-level")
               || p == QLatin1String("phantom") || p == QLatin1String("low-cut")) {
        auto &g = group(r, k + QStringLiteral("preamp"), {0.0f, 1.0f, 0.0f, 0.0f}); // gain, micLevel, phantom, lowCut
        if (p == QLatin1String("preamp")) g[0] = arg.toFloat();
        else if (p == QLatin1String("mic-level")) g[1] = arg.toBool();
        else if (p == QLatin1String("phantom")) g[2] = arg.toBool();
        else g[3] = arg.toBool();
        sw->setAnalogInputPreamp(id, g[0], g[1] != 0.0f, g[2] != 0.0f, g[3] != 0.0f);
    } else if (p == QLatin1String("eq/reset")) {
        for (int band = 0; band < 6; ++band) {
            r.params.remove(k + QStringLiteral("eq/%1").arg(band));
        }
        sw->resetFairlightEq(id);
    } else if (p.startsWith(QLatin1String("eq/"))) {
        static const QRegularExpression rxEq("^eq/(\\d+)/(.+)$");
        auto matchEq = rxEq.match(p);
        if (!matchEq.hasMatch()) return false;
        const int band = matchEq.captured(1).toInt();
        const QString q = matchEq.captured(2);
        auto &g = group(r, k + QStringLiteral("eq/%1").arg(band), {1.0f, 1000.0f, 0.0f, 0.71f}); // enabled, freq, gain, q
        if (q == QLatin1String("enabled")) g[0] = arg.toBool();
        else if (q == QLatin1String("freq")) g[1] = arg.toFloat();
        else if (q == QLatin1String("gain")) g[2] = arg.toFloat();
        else if (q == QLatin1String("q")) g[3] = arg.toFloat();
        else return false;
        sw->setFairlightEqBand(id, band, g[0] != 0.0f, g[1], g[2], g[3]);
    } else if (p.startsWith(QLatin1String("compressor/"))) {
        auto &g = group(r, k + QStringLiteral("comp"), {1.0f, -20.0f, 2.0f, 1.4f, 100.0f}); // enabled, thr, ratio, attack, release
        const QString q = p.mid(11);
        if (q == QLatin1String("enabled")) g[0] = arg.toBool();
        else if (q == QLatin1String("threshold")) g[1] = arg.toFloat();
        else if (q == QLatin1String("ratio")) g[2] = arg.toFloat();
        else if (q == QLatin1String("attack")) g[3] = arg.toFloat();
        else if (q == QLatin1String("release")) g[4] = arg.toFloat();
        else return false;
        sw->setFairlightCompressor(id, g[0] != 0.0f, g[1], g[2], g[3], g[4]);
    } else if (p.startsWith(QLatin1String("limiter/"))) {
        auto &g = group(r, k + QStringLiteral("lim"), {1.0f, -2.0f, 1.0f, 100.0f}); // enabled, thr, attack, release
        const QString q = p.mid(8);
        if (q == QLatin1String("enabled")) g[0] = arg.toBool();
        else if (q == QLatin1String("threshold")) g[1] = arg.toFloat();
        else if (q == QLatin1String("attack")) g[2] = arg.toFloat();
        else if (q == QLatin1String("release")) g[3] = arg.toFloat();
        else return false;
        sw->setFairlightLimiter(id, g[0] != 0.0f, g[1], g[2], g[3]);
    } else if (p.startsWith(QLatin1String("expander/"))) {
        auto &g = group(r, k + QStringLiteral("exp"), {1.0f, -40.0f, 18.0f, 2.0f}); // enabled, thr, range, ratio
        const QString q = p.mid(9);
        if (q == QLatin1String("enabled")) g[0] = arg.toBool();
        else if (q == QLatin1String("threshold")) g[1] = arg.toFloat();
        else if (q == QLatin1String("range")) g[2] = arg.toFloat();
        else if (q == QLatin1String("ratio")) g[3] = arg.toFloat();
        else return false;
        sw->setFairlightExpander(id, g[0] != 0.0f, g[1], g[2], g[3]);
    } else {
        return false;
    }
    return true;
}

bool Dispatcher::handleMultiView(SwitcherRoute &r, int mv, const QString &sub, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;

    static const QRegularExpression rxWin("^window/(\\d+)/source$");
    auto matchWin = rxWin.match(sub);
    if (matchWin.hasMatch()) {
        sw->setMultiViewWindowSource(mv, matchWin.captured(1).toInt(), arg.toInt());
    } else if (sub == QLatin1String("layout")) {
        sw->setMultiViewLayout(mv, parseMultiViewLayout(arg.toString()));
    } else if (sub == QLatin1String("safe-area")) {
        sw->setMultiViewSafeAreaVisible(mv, arg.toBool());
    } else if (sub == QLatin1String("meters")) {
        sw->setMultiViewMetersVisible(mv, arg.toBool());
    } else {
        return false;
    }
    return true;
}

bool Dispatcher::handleClock(SwitcherRoute &r, const QString &sub, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;

    if (sub == QLatin1String("enabled")) {
        sw->setDisplayClockEnabled(arg.toBool());
    } else if (sub == QLatin1String("mode")) {
        sw->setDisplayClockMode(parseClockMode(arg.toString()));
    } else if (sub == QLatin1String("start")) {
        sw->startDisplayClock();
    } else if (sub == QLatin1String("stop")) {
        sw->stopDisplayClock();
    } else if (sub == QLatin1String("reset")) {
        sw->resetDisplayClock();
    } else if (sub == QLatin1String("time")) {
        // HH:MM:SS, or MM:SS
        const QStringList parts = arg.toString().split(':');
        if (parts.size() == 3) sw->setDisplayClockTime(parts[0].toInt(), parts[1].toInt(), parts[2].toInt());
        else if (parts.size() == 2) sw->setDisplayClockTime(0, parts[0].toInt(), parts[1].toInt());
        else return false;
    } else if (sub == QLatin1String("size") || sub == QLatin1String("x")
               || sub == QLatin1String("y") || sub == QLatin1String("opacity")) {
        auto &g = group(r, QStringLiteral("clock/layout"), {1.0f, 0.5f, 0.5f, 1.0f}); // size, x, y, opacity
        if (sub == QLatin1String("size")) g[0] = arg.toFloat();
        else if (sub == QLatin1String("x")) g[1] = arg.toFloat();
        else if (sub == QLatin1String("y")) g[2] = arg.toFloat();
        else g[3] = arg.toFloat();
        sw->setDisplayClockLayout(g[0], g[1], g[2], g[3]);
    } else {
        return false;
    }
    return true;
}

bool Dispatcher::handleCamera(SwitcherRoute &r, int cam, const QString &sub, const Value &arg) {
    Atem::IAtemBackend *sw = r.backend;

    if (sub == QLatin1String("iris")) sw->setCameraIris(cam, arg.toFloat());
    else if (sub == QLatin1String("focus")) sw->setCameraFocus(cam, arg.toFloat());
    else if (sub == QLatin1String("autofocus")) sw->triggerCameraAutoFocus(cam);
    else if (sub == QLatin1String("gain")) sw->setCameraGain(cam, arg.toFloat());
    else if (sub == QLatin1String("shutter")) sw->setCameraShutter(cam, arg.toFloat());
    else if (sub == QLatin1String("white-balance")) sw->setCameraWhiteBalance(cam, arg.toFloat());
    else if (sub == QLatin1String("ptz/recall")) sw->recallCameraPtzPreset(cam, arg.toInt());
    else if (sub == QLatin1String("ptz/pan") || sub == QLatin1String("ptz/tilt") || sub == QLatin1String("ptz/zoom")) {
        auto &g = group(r, QStringLiteral("cam/%1/ptz").arg(cam), {0.0f, 0.0f, 0.0f}); // pan, tilt, zoom
        if (sub == QLatin1String("ptz/pan")) g[0] = arg.toFloat();
        else if (sub == QLatin1String("ptz/tilt")) g[1] = arg.toFloat();
        else g[2] = arg.toFloat();
        sw->setCameraPtz(cam, g[0], g[1], g[2]);
    } else {
        return false;
    }
    return true;
}

void Dispatcher::onConnectionStateChanged(Atem::ConnectionState state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    const bool connected = (state == Atem::ConnectionState::Connected);
    sendFeedback(backend, connectionLedAddress(true), { Value(connected ? 1.0f : 0.0f) });
    sendFeedback(backend, connectionLedAddress(false), { Value(connected ? 0.0f : 1.0f) });
}

// ================= Feedback slots from IAtemBackend =================

void Dispatcher::onMeStateChanged(const Atem::MeState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    QString prefix = QStringLiteral("/me/%1").arg(state.index);
    sendFeedback(backend, prefix + QStringLiteral("/program"), { Value(static_cast<int32_t>(state.programInput)) });
    sendFeedback(backend, prefix + QStringLiteral("/preview"), { Value(static_cast<int32_t>(state.previewInput)) });
    sendFeedback(backend, prefix + QStringLiteral("/transition/position"), { Value(state.transitionPosition) });
    sendFeedback(backend, prefix + QStringLiteral("/transition/ftb"), { Value(state.ftbActive) });
    {
        static const char *styleNames[] = { "mix", "dip", "wipe", "dve", "sting", "mix" };
        sendFeedback(backend, prefix + QStringLiteral("/transition/type"), { Value(QString::fromLatin1(styleNames[static_cast<int>(state.transitionStyle)])) });
        sendFeedback(backend, prefix + QStringLiteral("/transition/rate"), { Value(state.transitionRate) });
        sendFeedback(backend, prefix + QStringLiteral("/transition/preview"), { Value(state.previewTransition) });
    }
    {
        const auto *route = routeFor(backend);
        const float bar = (route && route->invertTBar) ? 1.0f - state.transitionPosition : state.transitionPosition;
        sendFeedback(backend, prefix + QStringLiteral("/transition/bar"), { Value(bar) });
        if (state.index == 1) {
            // Default M/E aliases
            sendFeedback(backend, QStringLiteral("/transition/bar"), { Value(bar) });
            sendFeedback(backend, QStringLiteral("/transition/position"), { Value(state.transitionPosition) });
        }
    }

    if (m_registry) {
        m_registry->updateLiveValue(prefix + QStringLiteral("/program"), QString::number(state.programInput));
        m_registry->updateLiveValue(prefix + QStringLiteral("/preview"), QString::number(state.previewInput));
        m_registry->updateLiveValue(prefix + QStringLiteral("/transition/position"), QString::number(state.transitionPosition, 'f', 3));
    }
}

void Dispatcher::onUskStateChanged(const Atem::KeyerState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    if (auto *route = routeFor(backend)) {
        const QString k = QStringLiteral("usk/%1/%2/").arg(state.meIndex).arg(state.keyerIndex);
        route->params[k + QStringLiteral("state")] = { state.onAir ? 1.0f : 0.0f, state.tie ? 1.0f : 0.0f };
        route->params[k + QStringLiteral("mask")] = { state.masked ? 1.0f : 0.0f, state.maskTop, state.maskBottom, state.maskLeft, state.maskRight };
        route->params[k + QStringLiteral("fly-pos")] = { state.posX, state.posY };
        route->params[k + QStringLiteral("fly-size")] = { state.sizeX, state.sizeY };
        route->params[k + QStringLiteral("luma")] = { state.lumaPreMultiplied ? 1.0f : 0.0f, state.lumaClip, state.lumaGain, state.lumaInverse ? 1.0f : 0.0f };
        route->params[k + QStringLiteral("chroma")] = { state.chromaHue, state.chromaGain, state.chromaYSuppress, state.chromaLift, state.chromaNarrow ? 1.0f : 0.0f };
    }

    QString prefix = QStringLiteral("/me/%1/usk/%2").arg(state.meIndex).arg(state.keyerIndex);
    sendFeedback(backend, prefix + QStringLiteral("/on-air"), { Value(state.onAir) });
    sendFeedback(backend, prefix + QStringLiteral("/tie"), { Value(state.tie) });
    sendFeedback(backend, prefix + QStringLiteral("/source/fill"), { Value(static_cast<int32_t>(state.fillInput)) });
    sendFeedback(backend, prefix + QStringLiteral("/source/cut"), { Value(static_cast<int32_t>(state.cutInput)) });
    {
        static const char *typeNames[] = { "luma", "chroma", "pattern", "dve", "chroma" };
        sendFeedback(backend, prefix + QStringLiteral("/type"), { Value(QString::fromLatin1(typeNames[static_cast<int>(state.type)])) });
    }
    sendFeedback(backend, prefix + QStringLiteral("/luma/pre-multiplied"), { Value(state.lumaPreMultiplied) });
    sendFeedback(backend, prefix + QStringLiteral("/luma/clip"), { Value(state.lumaClip) });
    sendFeedback(backend, prefix + QStringLiteral("/luma/gain"), { Value(state.lumaGain) });
    sendFeedback(backend, prefix + QStringLiteral("/luma/inverse"), { Value(state.lumaInverse) });
    sendFeedback(backend, prefix + QStringLiteral("/chroma/hue"), { Value(state.chromaHue) });
    sendFeedback(backend, prefix + QStringLiteral("/chroma/gain"), { Value(state.chromaGain) });
    sendFeedback(backend, prefix + QStringLiteral("/chroma/y-suppress"), { Value(state.chromaYSuppress) });
    sendFeedback(backend, prefix + QStringLiteral("/chroma/lift"), { Value(state.chromaLift) });
    sendFeedback(backend, prefix + QStringLiteral("/chroma/narrow"), { Value(state.chromaNarrow) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/enabled"), { Value(state.masked) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/top"), { Value(state.maskTop) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/bottom"), { Value(state.maskBottom) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/left"), { Value(state.maskLeft) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/right"), { Value(state.maskRight) });
    sendFeedback(backend, prefix + QStringLiteral("/fly/enable"), { Value(state.flyEnabled) });
    sendFeedback(backend, prefix + QStringLiteral("/fly/position-x"), { Value(state.posX) });
    sendFeedback(backend, prefix + QStringLiteral("/fly/position-y"), { Value(state.posY) });
    sendFeedback(backend, prefix + QStringLiteral("/fly/size-x"), { Value(state.sizeX) });
    sendFeedback(backend, prefix + QStringLiteral("/fly/size-y"), { Value(state.sizeY) });
    sendFeedback(backend, prefix + QStringLiteral("/fly/rotation"), { Value(state.rotation) });

    if (m_registry) {
        m_registry->updateLiveValue(prefix + QStringLiteral("/on-air"), state.onAir ? QStringLiteral("ON") : QStringLiteral("OFF"));
        m_registry->updateLiveValue(prefix + QStringLiteral("/tie"), state.tie ? QStringLiteral("TIED") : QStringLiteral("UNTIED"));
    }
}

void Dispatcher::onDskStateChanged(const Atem::DskState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    if (auto *route = routeFor(backend)) {
        const QString k = QStringLiteral("dsk/%1/").arg(state.index);
        route->params[k + QStringLiteral("state")] = { state.onAir ? 1.0f : 0.0f, state.tie ? 1.0f : 0.0f };
        route->params[k + QStringLiteral("luma")] = { state.preMultiplied ? 1.0f : 0.0f, state.clip, state.gain, state.inverse ? 1.0f : 0.0f };
        route->params[k + QStringLiteral("mask")] = { state.masked ? 1.0f : 0.0f, state.maskTop, state.maskBottom, state.maskLeft, state.maskRight };
    }

    QString prefix = QStringLiteral("/dsk/%1").arg(state.index);
    sendFeedback(backend, prefix + QStringLiteral("/on-air"), { Value(state.onAir) });
    sendFeedback(backend, prefix + QStringLiteral("/tie"), { Value(state.tie) });
    sendFeedback(backend, prefix + QStringLiteral("/rate"), { Value(state.rate) });
    sendFeedback(backend, prefix + QStringLiteral("/source/fill"), { Value(static_cast<int32_t>(state.fillInput)) });
    sendFeedback(backend, prefix + QStringLiteral("/source/cut"), { Value(static_cast<int32_t>(state.cutInput)) });
    sendFeedback(backend, prefix + QStringLiteral("/clip"), { Value(state.clip) });
    sendFeedback(backend, prefix + QStringLiteral("/gain"), { Value(state.gain) });
    sendFeedback(backend, prefix + QStringLiteral("/pre-multiplied"), { Value(state.preMultiplied) });
    sendFeedback(backend, prefix + QStringLiteral("/inverse"), { Value(state.inverse) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/enabled"), { Value(state.masked) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/top"), { Value(state.maskTop) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/bottom"), { Value(state.maskBottom) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/left"), { Value(state.maskLeft) });
    sendFeedback(backend, prefix + QStringLiteral("/mask/right"), { Value(state.maskRight) });

    if (m_registry) {
        m_registry->updateLiveValue(prefix + QStringLiteral("/on-air"), state.onAir ? QStringLiteral("ON") : QStringLiteral("OFF"));
        m_registry->updateLiveValue(prefix + QStringLiteral("/tie"), state.tie ? QStringLiteral("TIED") : QStringLiteral("UNTIED"));
    }
}

void Dispatcher::onSuperSourceBoxChanged(const Atem::SuperSourceBoxState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    QString prefix = QStringLiteral("/supersource/box/%1").arg(state.boxIndex);
    sendFeedback(backend, prefix + QStringLiteral("/enabled"), { Value(state.enabled) });
    sendFeedback(backend, prefix + QStringLiteral("/source"), { Value(static_cast<int32_t>(state.inputSource)) });
    sendFeedback(backend, prefix + QStringLiteral("/x"), { Value(state.x) });
    sendFeedback(backend, prefix + QStringLiteral("/y"), { Value(state.y) });
    sendFeedback(backend, prefix + QStringLiteral("/size"), { Value(state.size) });
    if (auto *route = routeFor(backend)) {
        const QString k = QStringLiteral("ssrc/%1/").arg(state.boxIndex);
        route->params[k + QStringLiteral("pos")] = { state.x, state.y };
        route->params[k + QStringLiteral("crop")] = { state.cropped ? 1.0f : 0.0f, state.cropTop, state.cropBottom, state.cropLeft, state.cropRight };
    }

    if (m_registry) {
        m_registry->updateLiveValue(prefix + QStringLiteral("/enabled"), state.enabled ? QStringLiteral("true") : QStringLiteral("false"));
    }
}

void Dispatcher::onAudioChannelChanged(const Atem::AudioChannelState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    QString prefix = QStringLiteral("/audio/input/%1").arg(state.channelId);
    sendFeedback(backend, prefix + QStringLiteral("/gain"), { Value(state.faderGainDb) });
    sendFeedback(backend, prefix + QStringLiteral("/balance"), { Value(state.pan) });
    sendFeedback(backend, prefix + QStringLiteral("/solo"), { Value(state.solo) });
    {
        const QString mix = state.mixOption == Atem::AudioMixOption::On ? QStringLiteral("on")
                          : state.mixOption == Atem::AudioMixOption::AudioFollowVideo ? QStringLiteral("afv") : QStringLiteral("off");
        sendFeedback(backend, prefix + QStringLiteral("/mix"), { Value(mix) });
        sendFeedback(backend, prefix + QStringLiteral("/mix/afv"), { Value(mix == QLatin1String("afv") ? 1.0f : 0.0f) });
        sendFeedback(backend, prefix + QStringLiteral("/mix/on"), { Value(mix == QLatin1String("on") ? 1.0f : 0.0f) });
        sendFeedback(backend, prefix + QStringLiteral("/mix/off"), { Value(mix == QLatin1String("off") ? 1.0f : 0.0f) });
    }

    if (m_registry) {
        m_registry->updateLiveValue(prefix + QStringLiteral("/gain"), QString::number(state.faderGainDb, 'f', 1) + QStringLiteral(" dB"));
    }
}

void Dispatcher::onAudioMasterGainChanged(float gainDb) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;
    sendFeedback(backend, QStringLiteral("/audio/output/gain"), { Value(gainDb) });
    if (m_registry) {
        m_registry->updateLiveValue(QStringLiteral("/audio/output/gain"), QString::number(gainDb, 'f', 1) + QStringLiteral(" dB"));
    }
}

void Dispatcher::onFairlightEqBandChanged(const Atem::FairlightEqBandState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    if (auto *route = routeFor(backend)) {
        route->params[QStringLiteral("aud/%1/eq/%2").arg(state.channelId).arg(state.bandIndex)] =
            { state.enabled ? 1.0f : 0.0f, state.frequency, state.gain, state.qFactor };
    }

    QString prefix = QStringLiteral("/audio/input/%1/eq/%2").arg(state.channelId).arg(state.bandIndex);
    sendFeedback(backend, prefix + QStringLiteral("/gain"), { Value(state.gain) });
    sendFeedback(backend, prefix + QStringLiteral("/freq"), { Value(state.frequency) });
    sendFeedback(backend, prefix + QStringLiteral("/q"), { Value(state.qFactor) });
    sendFeedback(backend, prefix + QStringLiteral("/enabled"), { Value(state.enabled) });
}

void Dispatcher::onFairlightDynamicsChanged(const Atem::FairlightDynamicsState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    if (auto *route = routeFor(backend)) {
        const QString k = QStringLiteral("aud/%1/").arg(state.channelId);
        route->params[k + QStringLiteral("comp")] = { state.compEnabled ? 1.0f : 0.0f, state.compThreshold, state.compRatio, state.compAttack, state.compRelease };
        route->params[k + QStringLiteral("exp")] = { state.expEnabled ? 1.0f : 0.0f, state.expThreshold, state.expRange, state.expRatio };
    }

    QString prefix = QStringLiteral("/audio/input/%1").arg(state.channelId);
    sendFeedback(backend, prefix + QStringLiteral("/compressor/threshold"), { Value(state.compThreshold) });
    sendFeedback(backend, prefix + QStringLiteral("/compressor/ratio"), { Value(state.compRatio) });
    sendFeedback(backend, prefix + QStringLiteral("/limiter/threshold"), { Value(state.limThreshold) });
}

void Dispatcher::onMultiViewChanged(const Atem::MultiViewState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    QString prefix = QStringLiteral("/multiview/%1").arg(state.multiViewIndex);
    for (auto it = state.windowSources.constBegin(); it != state.windowSources.constEnd(); ++it) {
        sendFeedback(backend, prefix + QStringLiteral("/window/%1/source").arg(it.key()), { Value(static_cast<int32_t>(it.value())) });
    }
    sendFeedback(backend, prefix + QStringLiteral("/safe-area"), { Value(state.safeAreaVisible) });
    sendFeedback(backend, prefix + QStringLiteral("/meters"), { Value(state.metersVisible) });
}

void Dispatcher::onDisplayClockChanged(const Atem::DisplayClockState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    if (auto *route = routeFor(backend)) {
        route->params[QStringLiteral("clock/layout")] = { state.size, state.x, state.y, state.opacity };
    }

    sendFeedback(backend, QStringLiteral("/clock/enabled"), { Value(state.enabled) });
    sendFeedback(backend, QStringLiteral("/clock/size"), { Value(state.size) });
    sendFeedback(backend, QStringLiteral("/clock/opacity"), { Value(state.opacity) });
    sendFeedback(backend, QStringLiteral("/clock/x"), { Value(state.x) });
    sendFeedback(backend, QStringLiteral("/clock/y"), { Value(state.y) });
    {
        static const char *modes[] = { "timeofday", "countdown", "countup" };
        sendFeedback(backend, QStringLiteral("/clock/mode"), { Value(QString::fromLatin1(modes[static_cast<int>(state.mode)])) });
    }
    sendFeedback(backend, QStringLiteral("/clock/running"), { Value(state.isRunning) });
    sendFeedback(backend, QStringLiteral("/clock/time"),
                 { Value(QStringLiteral("%1:%2:%3").arg(state.hours, 2, 10, QChar('0')).arg(state.minutes, 2, 10, QChar('0')).arg(state.seconds, 2, 10, QChar('0'))) });
}

void Dispatcher::onRecordStateChanged(const Atem::RecordState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    sendFeedback(backend, QStringLiteral("/recording/status"), { Value(state.isRecording) });
    sendFeedback(backend, QStringLiteral("/recording/active"), { Value(state.isRecording) });
    sendFeedback(backend, QStringLiteral("/recording/state"), { Value(state.isRecording ? QStringLiteral("recording") : QStringLiteral("idle")) });
    sendFeedback(backend, QStringLiteral("/recording/filename"), { Value(state.filename) });
    sendFeedback(backend, QStringLiteral("/recording/duration"), { Value(static_cast<int32_t>(state.durationSeconds)) });
    sendFeedback(backend, QStringLiteral("/recording/iso"), { Value(state.recordAllInputsIso) });

    if (m_registry) {
        m_registry->updateLiveValue(QStringLiteral("/recording/status"), state.isRecording ? QStringLiteral("Recording") : QStringLiteral("Stopped"));
    }
}

void Dispatcher::onStreamStateChanged(const Atem::StreamState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    sendFeedback(backend, QStringLiteral("/stream/status"), { Value(state.isStreaming) });
    sendFeedback(backend, QStringLiteral("/stream/active"), { Value(state.isStreaming) });
    sendFeedback(backend, QStringLiteral("/stream/state"), { Value(state.isStreaming ? QStringLiteral("streaming") : QStringLiteral("idle")) });
    sendFeedback(backend, QStringLiteral("/stream/bitrate"), { Value(static_cast<int32_t>(state.bitrate)) });
    sendFeedback(backend, QStringLiteral("/stream/cache"), { Value(state.cachePercentage) });

    if (m_registry) {
        m_registry->updateLiveValue(QStringLiteral("/stream/status"), state.isStreaming ? QStringLiteral("Streaming") : QStringLiteral("Stopped"));
    }
}

void Dispatcher::onMacroStateChanged(const Atem::MacroState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    QString prefix = QStringLiteral("/macros/%1").arg(state.index);
    sendFeedback(backend, prefix + QStringLiteral("/is-valid"), { Value(state.isValid) });
    sendFeedback(backend, prefix + QStringLiteral("/name"), { Value(state.name) });
    sendFeedback(backend, prefix + QStringLiteral("/description"), { Value(state.description) });
    sendFeedback(backend, prefix + QStringLiteral("/status"), { Value(state.isRunning ? QStringLiteral("running") : state.isWaiting ? QStringLiteral("waiting") : QStringLiteral("idle")) });
}

void Dispatcher::onCameraControlChanged(const Atem::CameraControlState &state) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    QString prefix = QStringLiteral("/camera/%1").arg(state.cameraId);
    sendFeedback(backend, prefix + QStringLiteral("/iris"), { Value(state.iris) });
    sendFeedback(backend, prefix + QStringLiteral("/focus"), { Value(state.focus) });
    sendFeedback(backend, prefix + QStringLiteral("/gain"), { Value(state.gain) });
    sendFeedback(backend, prefix + QStringLiteral("/shutter"), { Value(state.shutter) });
    sendFeedback(backend, prefix + QStringLiteral("/white-balance"), { Value(state.whiteBalance) });
}

void Dispatcher::onAuxSourceChanged(int auxIndex, int64_t inputId) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    sendFeedback(backend, QStringLiteral("/aux/%1").arg(auxIndex), { Value(static_cast<int32_t>(inputId)) });
    if (m_registry) {
        m_registry->updateLiveValue(QStringLiteral("/aux/%1").arg(auxIndex), QString::number(inputId));
    }
}

void Dispatcher::onInputChanged(const Atem::InputInfo &info) {
    auto *backend = qobject_cast<Atem::IAtemBackend*>(sender());
    if (!backend) return;

    QString prefix = QStringLiteral("/input/%1").arg(info.id);
    sendFeedback(backend, prefix + QStringLiteral("/short-name"), { Value(info.shortName) });
    sendFeedback(backend, prefix + QStringLiteral("/long-name"), { Value(info.longName) });
}

} // namespace Osc
