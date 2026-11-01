#include "NetworkAtemBackend.h"
#include <QNetworkDatagram>
#include <QtEndian>
#include <cmath>
#include <cstring>

namespace AtemNet {

NetworkAtemBackend::NetworkAtemBackend(QObject *parent)
    : Atem::IAtemBackend(parent)
{
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(500);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &NetworkAtemBackend::sendHeartbeat);
}

NetworkAtemBackend::~NetworkAtemBackend() {
    disconnectFromSwitcher();
}

void NetworkAtemBackend::connectTo(const QString &ip) {
    disconnectFromSwitcher();
    m_ip = ip;
    m_sessionId = 0;
    m_lastRemotePacketId = 0;
    m_localPacketId = 0;
    m_inputsMap.clear();
    m_meMap.clear();
    m_nextSelection.clear();
    m_multiViews.clear();
    m_connectionState = Atem::ConnectionState::Connecting;
    emit connectionStateChanged(m_connectionState);
    emit logMessage(QStringLiteral("Connecting to ATEM switcher at %1:9910 (Direct UDP)...").arg(ip));

    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &NetworkAtemBackend::onReadyRead);

    m_lastRx.restart();
    m_socket->writeDatagram(Packet::createHello(), QHostAddress(m_ip), m_port);
    m_heartbeatTimer->start();
}

void NetworkAtemBackend::disconnectFromSwitcher() {
    if (m_heartbeatTimer) m_heartbeatTimer->stop();
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (m_connectionState != Atem::ConnectionState::Disconnected) {
        m_connectionState = Atem::ConnectionState::Disconnected;
        emit connectionStateChanged(m_connectionState);
        emit logMessage(QStringLiteral("Disconnected from switcher."));
    }
}

void NetworkAtemBackend::failConnection(const QString &reason) {
    m_heartbeatTimer->stop();
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_connectionState = Atem::ConnectionState::Failed;
    emit connectionStateChanged(m_connectionState);
    emit logMessage(reason);
}

void NetworkAtemBackend::unsupported(const char *feature) {
    const QString name = QString::fromLatin1(feature);
    if (m_unsupportedLogged.contains(name)) return;
    m_unsupportedLogged.insert(name);
    emit logMessage(QStringLiteral("%1 is not available with the direct network backend; use the official SDK backend (macOS/Windows).").arg(name));
}

void NetworkAtemBackend::sendHeartbeat() {
    if (!m_socket) return;

    if (m_connectionState == Atem::ConnectionState::Connecting) {
        if (m_lastRx.elapsed() > 5000 && m_sessionId == 0) {
            failConnection(QStringLiteral("No response from ATEM switcher at %1:9910.").arg(m_ip));
            return;
        }
        if (m_sessionId == 0) {
            m_socket->writeDatagram(Packet::createHello(), QHostAddress(m_ip), m_port);
        }
    } else if (m_connectionState == Atem::ConnectionState::Connected) {
        // The switcher pings every second; silence means the link is gone
        if (m_lastRx.elapsed() > 5000) {
            failConnection(QStringLiteral("Lost connection to ATEM switcher (no packets for 5 s)."));
        }
    }
}

void NetworkAtemBackend::onReadyRead() {
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();

        PacketHeader header;
        if (!Packet::parseHeader(data, header)) continue;
        m_lastRx.restart();

        // Hello answer: carries the session id for this connection
        if (header.flags & FlagNewSession) {
            m_sessionId = header.sessionId;
            m_lastRemotePacketId = 0;
            m_socket->writeDatagram(Packet::createAck(m_sessionId, 0), QHostAddress(m_ip), m_port);
            continue;
        }

        if (header.sessionId != m_sessionId) continue;

        if (header.flags & FlagAckRequest) {
            const uint16_t expected = (m_lastRemotePacketId + 1) & 0x7fff;
            if (header.packetId == expected) {
                m_lastRemotePacketId = header.packetId;
            } else {
                // Duplicate or out of order: re-acknowledge the last good packet and wait for the resend
                m_socket->writeDatagram(Packet::createAck(m_sessionId, m_lastRemotePacketId), QHostAddress(m_ip), m_port);
                continue;
            }
            m_socket->writeDatagram(Packet::createAck(m_sessionId, header.packetId), QHostAddress(m_ip), m_port);

            if (header.length > 12) {
                parseCommands(data.constData() + 12, header.length - 12);
            }
        }
    }
}

static uint16_t be16(const char *p) {
    return static_cast<uint16_t>((static_cast<uint8_t>(p[0]) << 8) | static_cast<uint8_t>(p[1]));
}

// Fixed-size, NUL padded text field
static QString fixedString(const char *p, int maxLen) {
    int len = 0;
    while (len < maxLen && p[len] != '\0') ++len;
    return QString::fromUtf8(p, len).trimmed();
}

void NetworkAtemBackend::parseCommands(const char *data, int length) {
    int offset = 0;
    while (offset + 8 <= length) {
        const uint16_t cmdLen = be16(data + offset);
        if (cmdLen < 8 || offset + cmdLen > length) break;

        const QByteArray name(data + offset + 4, 4);
        const char *payload = data + offset + 8;
        const int payloadLen = cmdLen - 8;

        if (name == "InCm") {
            // Initial state dump is complete
            if (m_connectionState != Atem::ConnectionState::Connected) {
                m_connectionState = Atem::ConnectionState::Connected;
                emit connectionStateChanged(m_connectionState);
                emit logMessage(QStringLiteral("Connected to %1 via UDP protocol (session 0x%2)")
                                    .arg(m_switcherName).arg(m_sessionId, 4, 16, QChar('0')));
            }
        } else if (name == "_pin" && payloadLen > 0) {
            m_switcherName = fixedString(payload, qMin(payloadLen, 44));
            emit switcherNameChanged(m_switcherName);
        } else if (name == "InPr" && payloadLen >= 26) {
            Atem::InputInfo info;
            info.id = be16(payload);
            info.longName = fixedString(payload + 2, 20);
            info.shortName = fixedString(payload + 22, 4);
            m_inputsMap[info.id] = info;
            emit inputChanged(info);
            emit inputListChanged();
        } else if (name == "PrgI" && payloadLen >= 4) {
            const int me = static_cast<uint8_t>(payload[0]) + 1;
            auto &state = m_meMap[me];
            state.index = me;
            state.programInput = be16(payload + 2);
            emit meStateChanged(state);
        } else if (name == "PrvI" && payloadLen >= 4) {
            const int me = static_cast<uint8_t>(payload[0]) + 1;
            auto &state = m_meMap[me];
            state.index = me;
            state.previewInput = be16(payload + 2);
            emit meStateChanged(state);
        } else if (name == "TrPs" && payloadLen >= 6) {
            const int me = static_cast<uint8_t>(payload[0]) + 1;
            auto &state = m_meMap[me];
            state.index = me;
            state.inTransition = payload[1] != 0;
            state.transitionFramesRemaining = static_cast<uint8_t>(payload[2]);
            state.transitionPosition = be16(payload + 4) / 10000.0f;
            emit meStateChanged(state);
        } else if (name == "TrSt" && payloadLen >= 5) {
            const int me = static_cast<uint8_t>(payload[0]) + 1;
            auto &state = m_meMap[me];
            state.index = me;
            static const Atem::TransitionStyle styles[] = {
                Atem::TransitionStyle::Mix, Atem::TransitionStyle::Dip, Atem::TransitionStyle::Wipe,
                Atem::TransitionStyle::DVE, Atem::TransitionStyle::Stinger };
            const uint8_t style = static_cast<uint8_t>(payload[1]);
            state.transitionStyle = style < 5 ? styles[style] : Atem::TransitionStyle::Unknown;
            m_nextSelection[me] = static_cast<uint8_t>(payload[4]);
            emit meStateChanged(state);
        } else if (name == "FtbS" && payloadLen >= 3) {
            const int me = static_cast<uint8_t>(payload[0]) + 1;
            auto &state = m_meMap[me];
            state.index = me;
            state.ftbActive = payload[1] != 0;
            emit meStateChanged(state);
        } else if (name == "AuxS" && payloadLen >= 4) {
            emit auxSourceChanged(static_cast<uint8_t>(payload[0]) + 1, be16(payload + 2));
        } else if (name == "MvIn" && payloadLen >= 4) {
            const int mv = static_cast<uint8_t>(payload[0]) + 1;
            auto &state = m_multiViews[mv];
            state.multiViewIndex = mv;
            state.windowSources[static_cast<uint8_t>(payload[1]) + 1] = be16(payload + 2);
            emit multiViewChanged(state);
        }

        offset += cmdLen;
    }
}

void NetworkAtemBackend::sendCommand(const char name[5], const QByteArray &payload) {
    if (!m_socket || m_sessionId == 0 || m_connectionState != Atem::ConnectionState::Connected) return;

    const uint16_t cmdLen = static_cast<uint16_t>(8 + payload.size());
    QByteArray cmd(8, 0);
    cmd[0] = static_cast<char>((cmdLen >> 8) & 0xff);
    cmd[1] = static_cast<char>(cmdLen & 0xff);
    std::memcpy(cmd.data() + 4, name, 4);
    cmd.append(payload);

    m_socket->writeDatagram(Packet::createCommandPacket(m_sessionId, m_localPacketId, cmd),
                            QHostAddress(m_ip), m_port);
}

void NetworkAtemBackend::requestStatus() {
    // Switcher sends full state upon connection handshake
}

void NetworkAtemBackend::setProgramInput(int me, int64_t inputId) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    sendCommand("CPgI", payload);
}

void NetworkAtemBackend::setPreviewInput(int me, int64_t inputId) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    sendCommand("CPvI", payload);
}

void NetworkAtemBackend::performCut(int me) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    sendCommand("DCut", payload);
}

void NetworkAtemBackend::performAutoTransition(int me) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    sendCommand("DAut", payload);
}

void NetworkAtemBackend::performFadeToBlack(int me) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    sendCommand("FtbA", payload);
}

void NetworkAtemBackend::setTransitionPosition(int me, float position) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    uint16_t pos = static_cast<uint16_t>(qBound(0.0f, position, 1.0f) * 10000.0f);
    payload[2] = static_cast<char>((pos >> 8) & 0xff);
    payload[3] = static_cast<char>(pos & 0xff);
    sendCommand("CTPs", payload);
}

void NetworkAtemBackend::setTransitionStyle(int me, Atem::TransitionStyle style) {
    uint8_t s = 0; // mix
    if (style == Atem::TransitionStyle::Dip) s = 1;
    else if (style == Atem::TransitionStyle::Wipe) s = 2;
    else if (style == Atem::TransitionStyle::DVE) s = 3;
    else if (style == Atem::TransitionStyle::Stinger) s = 4;
    QByteArray payload(4, 0);
    payload[0] = 0x01; // mask: style
    payload[1] = static_cast<char>(me - 1);
    payload[2] = static_cast<char>(s);
    sendCommand("CTTp", payload);
}

void NetworkAtemBackend::setTransitionRate(int me, Atem::TransitionStyle style, float rateFrames) {
    const uint8_t rate = static_cast<uint8_t>(qBound(1, qRound(rateFrames), 250));
    switch (style) {
        case Atem::TransitionStyle::Mix: {
            QByteArray payload(4, 0);
            payload[0] = static_cast<char>(me - 1);
            payload[1] = static_cast<char>(rate);
            sendCommand("CTMx", payload);
            break;
        }
        case Atem::TransitionStyle::Dip: {
            QByteArray payload(8, 0);
            payload[0] = 0x01; // mask: rate
            payload[1] = static_cast<char>(me - 1);
            payload[2] = static_cast<char>(rate);
            sendCommand("CTDp", payload);
            break;
        }
        case Atem::TransitionStyle::Wipe: {
            QByteArray payload(20, 0);
            payload[0] = 0x01; // mask: rate
            payload[1] = static_cast<char>(me - 1);
            payload[2] = static_cast<char>(rate);
            sendCommand("CTWp", payload);
            break;
        }
        case Atem::TransitionStyle::DVE: {
            QByteArray payload(20, 0);
            payload[0] = 0x01; // mask: rate
            payload[1] = static_cast<char>(me - 1);
            payload[2] = static_cast<char>(rate);
            sendCommand("CTDv", payload);
            break;
        }
        default:
            unsupported("setTransitionRate (this transition style)");
            break;
    }
}

void NetworkAtemBackend::setPreviewTransition(int me, bool preview) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    payload[1] = preview ? 1 : 0;
    sendCommand("CTPr", payload);
}

void NetworkAtemBackend::setUskOnAir(int me, int keyer, bool onAir) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    payload[1] = static_cast<char>(keyer - 1);
    payload[2] = onAir ? 1 : 0;
    sendCommand("CKOn", payload);
}

void NetworkAtemBackend::setUskTie(int me, int keyer, bool tie) {
    // The "tie" of a key is its bit in the next-transition selection (keyer 0 = background)
    uint8_t selection = m_nextSelection.value(me, 0x01);
    const uint8_t bit = static_cast<uint8_t>(1 << keyer);
    selection = tie ? (selection | bit) : (selection & ~bit);
    if ((selection & 0x1f) == 0) selection = 0x01; // at least one layer must be selected
    m_nextSelection[me] = selection;

    QByteArray payload(4, 0);
    payload[0] = 0x02; // mask: selection
    payload[1] = static_cast<char>(me - 1);
    payload[3] = static_cast<char>(selection);
    sendCommand("CTTp", payload);
}

void NetworkAtemBackend::setUskType(int me, int keyer, Atem::KeyType type) {
    if (type == Atem::KeyType::AdvancedChroma) {
        unsupported("setUskType (advanced chroma)");
        return;
    }
    QByteArray payload(8, 0);
    payload[0] = 0x01; // mask: type
    payload[1] = static_cast<char>(me - 1);
    payload[2] = static_cast<char>(keyer - 1);
    payload[3] = static_cast<char>(type); // luma, chroma, pattern, dve
    sendCommand("CKTp", payload);
}

void NetworkAtemBackend::setUskFillSource(int me, int keyer, int64_t inputId) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    payload[1] = static_cast<char>(keyer - 1);
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    sendCommand("CKeF", payload);
}

void NetworkAtemBackend::setUskCutSource(int me, int keyer, int64_t inputId) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(me - 1);
    payload[1] = static_cast<char>(keyer - 1);
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    sendCommand("CKeC", payload);
}

void NetworkAtemBackend::setUskLumaParams(int me, int keyer, bool preMult, float clip, float gain, bool inverse) {
    unsupported("setUskLumaParams");
}

void NetworkAtemBackend::setUskChromaParams(int me, int keyer, float hue, float gain, float ySuppress, float lift, bool narrow) {
    unsupported("setUskChromaParams");
}

void NetworkAtemBackend::setUskAdvancedChromaSample(int me, int keyer, float x, float y) {
    unsupported("setUskAdvancedChromaSample");
}

void NetworkAtemBackend::setUskFlyEnabled(int me, int keyer, bool enabled) {
    unsupported("setUskFlyEnabled");
}

void NetworkAtemBackend::setUskFlyPosition(int me, int keyer, float x, float y) {
    unsupported("setUskFlyPosition");
}

void NetworkAtemBackend::setUskFlySize(int me, int keyer, float sizeX, float sizeY) {
    unsupported("setUskFlySize");
}

void NetworkAtemBackend::setUskFlyRotation(int me, int keyer, float rotation) {
    unsupported("setUskFlyRotation");
}

void NetworkAtemBackend::resetUskFly(int me, int keyer) {
    unsupported("resetUskFly");
}

void NetworkAtemBackend::setUskMask(int me, int keyer, bool enabled, float top, float bottom, float left, float right) {
    unsupported("setUskMask");
}

void NetworkAtemBackend::resetUskMask(int me, int keyer) {
    unsupported("resetUskMask");
}

void NetworkAtemBackend::setDskOnAir(int keyer, bool onAir) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(keyer - 1);
    payload[1] = onAir ? 1 : 0;
    sendCommand("CDsL", payload);
}

void NetworkAtemBackend::setDskTie(int keyer, bool tie) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(keyer - 1);
    payload[1] = tie ? 1 : 0;
    sendCommand("CDsT", payload);
}

void NetworkAtemBackend::performDskAutoTransition(int keyer) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(keyer - 1);
    sendCommand("DDsA", payload);
}

void NetworkAtemBackend::setDskRate(int keyer, int rateFrames) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(keyer - 1);
    payload[1] = static_cast<char>(qBound(1, rateFrames, 250));
    sendCommand("CDsR", payload);
}

void NetworkAtemBackend::setDskFillSource(int keyer, int64_t inputId) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(keyer - 1);
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    sendCommand("CDsF", payload);
}

void NetworkAtemBackend::setDskCutSource(int keyer, int64_t inputId) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(keyer - 1);
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    sendCommand("CDsC", payload);
}

void NetworkAtemBackend::setDskLumaParams(int keyer, bool preMult, float clip, float gain, bool inverse) {
    unsupported("setDskLumaParams");
}

void NetworkAtemBackend::setDskMask(int keyer, bool enabled, float top, float bottom, float left, float right) {
    unsupported("setDskMask");
}

void NetworkAtemBackend::resetDskMask(int keyer) {
    unsupported("resetDskMask");
}

void NetworkAtemBackend::setSuperSourceBoxEnabled(int box, bool enabled) {
    unsupported("setSuperSourceBoxEnabled");
}

void NetworkAtemBackend::setSuperSourceBoxSource(int box, int64_t inputId) {
    unsupported("setSuperSourceBoxSource");
}

void NetworkAtemBackend::setSuperSourceBoxPosition(int box, float x, float y) {
    unsupported("setSuperSourceBoxPosition");
}

void NetworkAtemBackend::setSuperSourceBoxSize(int box, float size) {
    unsupported("setSuperSourceBoxSize");
}

void NetworkAtemBackend::setSuperSourceBoxCrop(int box, bool cropped, float top, float bottom, float left, float right) {
    unsupported("setSuperSourceBoxCrop");
}

void NetworkAtemBackend::resetSuperSourceBoxCrop(int box) {
    unsupported("resetSuperSourceBoxCrop");
}

void NetworkAtemBackend::setSuperSourceBorder(bool enabled, float widthIn, float widthOut, float softnessIn, float softnessOut,
                                              float hue, float sat, float luma) {
    unsupported("setSuperSourceBorder");
}

// Classic audio mixer gain: 0 dB = 32768, +6 dB = 65381, linear amplitude
static uint16_t audioGainToRaw(float db) {
    if (db <= -60.0f) return 0;
    const double raw = 32768.0 * std::pow(10.0, db / 20.0);
    return static_cast<uint16_t>(qBound(0.0, raw, 65381.0));
}

void NetworkAtemBackend::setAudioInputGain(int64_t inputId, float gainDb) {
    const uint16_t gain = audioGainToRaw(gainDb);
    QByteArray payload(12, 0);
    payload[0] = 0x02; // mask: gain
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    payload[6] = static_cast<char>((gain >> 8) & 0xff);
    payload[7] = static_cast<char>(gain & 0xff);
    sendCommand("CAMI", payload);
}

void NetworkAtemBackend::setAudioInputPan(int64_t inputId, float pan) {
    const int16_t balance = static_cast<int16_t>(qBound(-1.0f, pan, 1.0f) * 10000.0f);
    QByteArray payload(12, 0);
    payload[0] = 0x04; // mask: balance
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    payload[8] = static_cast<char>((balance >> 8) & 0xff);
    payload[9] = static_cast<char>(balance & 0xff);
    sendCommand("CAMI", payload);
}

void NetworkAtemBackend::setAudioInputMixOption(int64_t inputId, Atem::AudioMixOption option) {
    QByteArray payload(12, 0);
    payload[0] = 0x01; // mask: mix option
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    payload[4] = static_cast<char>(option); // off, on, afv
    sendCommand("CAMI", payload);
}

void NetworkAtemBackend::setAudioMasterGain(float gainDb) {
    const uint16_t gain = audioGainToRaw(gainDb);
    QByteArray payload(8, 0);
    payload[0] = 0x01; // mask: gain
    payload[2] = static_cast<char>((gain >> 8) & 0xff);
    payload[3] = static_cast<char>(gain & 0xff);
    sendCommand("CAMM", payload);
}

void NetworkAtemBackend::setAudioSolo(int64_t inputId, bool solo) {
    unsupported("setAudioSolo");
}

void NetworkAtemBackend::setFairlightEqBand(int64_t inputId, int band, bool enabled, float freq, float gainDb, float q) {
    unsupported("setFairlightEqBand");
}

void NetworkAtemBackend::resetFairlightEq(int64_t inputId) {
    unsupported("resetFairlightEq");
}

void NetworkAtemBackend::setFairlightCompressor(int64_t inputId, bool enabled, float thresholdDb, float ratio, float attackMs, float releaseMs) {
    unsupported("setFairlightCompressor");
}

void NetworkAtemBackend::setFairlightLimiter(int64_t inputId, bool enabled, float thresholdDb, float attackMs, float releaseMs) {
    unsupported("setFairlightLimiter");
}

void NetworkAtemBackend::setFairlightExpander(int64_t inputId, bool enabled, float thresholdDb, float rangeDb, float ratio) {
    unsupported("setFairlightExpander");
}

void NetworkAtemBackend::setAnalogInputPreamp(int64_t inputId, float gainDb, bool micLevel, bool phantom48V, bool lowCut) {
    unsupported("setAnalogInputPreamp");
}

void NetworkAtemBackend::setMultiViewLayout(int mvIndex, Atem::MultiViewLayout layout) {
    unsupported("setMultiViewLayout");
}

void NetworkAtemBackend::setMultiViewWindowSource(int mvIndex, int window, int64_t inputId) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(mvIndex - 1);
    payload[1] = static_cast<char>(window - 1);
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    sendCommand("CMvI", payload);
}

void NetworkAtemBackend::setMultiViewSafeAreaVisible(int mvIndex, bool visible) {
    unsupported("setMultiViewSafeAreaVisible");
}

void NetworkAtemBackend::setMultiViewMetersVisible(int mvIndex, bool visible) {
    unsupported("setMultiViewMetersVisible");
}

void NetworkAtemBackend::setDisplayClockEnabled(bool enabled) {
    unsupported("setDisplayClockEnabled");
}

void NetworkAtemBackend::setDisplayClockMode(Atem::ClockMode mode) {
    unsupported("setDisplayClockMode");
}

void NetworkAtemBackend::startDisplayClock() {
    unsupported("startDisplayClock");
}

void NetworkAtemBackend::stopDisplayClock() {
    unsupported("stopDisplayClock");
}

void NetworkAtemBackend::resetDisplayClock() {
    unsupported("resetDisplayClock");
}

void NetworkAtemBackend::setDisplayClockTime(int hours, int minutes, int seconds) {
    unsupported("setDisplayClockTime");
}

void NetworkAtemBackend::setDisplayClockLayout(float size, float x, float y, float opacity) {
    unsupported("setDisplayClockLayout");
}

void NetworkAtemBackend::startRecording() {
    QByteArray payload(4, 0);
    payload[0] = 1;
    sendCommand("RcTM", payload);
}

void NetworkAtemBackend::stopRecording() {
    sendCommand("RcTM", QByteArray(4, 0));
}

void NetworkAtemBackend::switchRecordingDisk() {
    unsupported("switchRecordingDisk");
}

void NetworkAtemBackend::setRecordingFilename(const QString &filename) {
    unsupported("setRecordingFilename");
}

void NetworkAtemBackend::setRecordAllInputsIso(bool recordIso) {
    unsupported("setRecordAllInputsIso");
}

void NetworkAtemBackend::startStreaming() {
    QByteArray payload(4, 0);
    payload[0] = 1;
    sendCommand("StrR", payload);
}

void NetworkAtemBackend::stopStreaming() {
    sendCommand("StrR", QByteArray(4, 0));
}

void NetworkAtemBackend::setStreamingService(const QString &serviceName, const QString &url, const QString &key) {
    unsupported("setStreamingService");
}

void NetworkAtemBackend::runMacro(uint32_t index) {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>((index >> 8) & 0xff);
    payload[1] = static_cast<char>(index & 0xff);
    payload[2] = 0; // run
    sendCommand("MAct", payload);
}

void NetworkAtemBackend::stopMacro() {
    QByteArray payload(4, 0);
    payload[0] = static_cast<char>(0xff);
    payload[1] = static_cast<char>(0xff);
    payload[2] = 1; // stop
    sendCommand("MAct", payload);
}

void NetworkAtemBackend::setMacroLoop(bool loop) {
    QByteArray payload(4, 0);
    payload[0] = 0x01; // mask: loop
    payload[2] = loop ? 1 : 0;
    sendCommand("MRCP", payload);
}

void NetworkAtemBackend::hyperDeckPlay(int deck) {
    unsupported("hyperDeckPlay");
}

void NetworkAtemBackend::hyperDeckStop(int deck) {
    unsupported("hyperDeckStop");
}

void NetworkAtemBackend::hyperDeckRecord(int deck) {
    unsupported("hyperDeckRecord");
}

void NetworkAtemBackend::hyperDeckShuttle(int deck, int speedPercent) {
    unsupported("hyperDeckShuttle");
}

void NetworkAtemBackend::hyperDeckJog(int deck, int frameCount) {
    unsupported("hyperDeckJog");
}

void NetworkAtemBackend::hyperDeckSelectClip(int deck, int clipIndex) {
    unsupported("hyperDeckSelectClip");
}

void NetworkAtemBackend::hyperDeckSetClipTime(int deck, const QString &timecode) {
    unsupported("hyperDeckSetClipTime");
}

void NetworkAtemBackend::hyperDeckSetTimelineTime(int deck, const QString &timecode) {
    unsupported("hyperDeckSetTimelineTime");
}

void NetworkAtemBackend::hyperDeckSetLooped(int deck, bool loop) {
    unsupported("hyperDeckSetLooped");
}

void NetworkAtemBackend::hyperDeckSetSingleClip(int deck, bool single) {
    unsupported("hyperDeckSetSingleClip");
}

void NetworkAtemBackend::setCameraIris(int camera, float value) {
    unsupported("setCameraIris");
}

void NetworkAtemBackend::setCameraFocus(int camera, float value) {
    unsupported("setCameraFocus");
}

void NetworkAtemBackend::triggerCameraAutoFocus(int camera) {
    unsupported("triggerCameraAutoFocus");
}

void NetworkAtemBackend::setCameraGain(int camera, float db) {
    unsupported("setCameraGain");
}

void NetworkAtemBackend::setCameraShutter(int camera, float speed) {
    unsupported("setCameraShutter");
}

void NetworkAtemBackend::setCameraWhiteBalance(int camera, float kelvin) {
    unsupported("setCameraWhiteBalance");
}

void NetworkAtemBackend::setCameraPtz(int camera, float pan, float tilt, float zoom) {
    unsupported("setCameraPtz");
}

void NetworkAtemBackend::recallCameraPtzPreset(int camera, int presetIndex) {
    unsupported("recallCameraPtzPreset");
}

void NetworkAtemBackend::selectMediaPlayerStill(int player, int stillIndex) {
    QByteArray payload(8, 0);
    payload[0] = 0x03; // mask: type + still
    payload[1] = static_cast<char>(player - 1);
    payload[2] = 1; // still
    payload[3] = static_cast<char>(stillIndex - 1);
    sendCommand("MPSS", payload);
}

void NetworkAtemBackend::selectMediaPlayerClip(int player, int clipIndex) {
    QByteArray payload(8, 0);
    payload[0] = 0x05; // mask: type + clip
    payload[1] = static_cast<char>(player - 1);
    payload[2] = 2; // clip
    payload[4] = static_cast<char>(clipIndex - 1);
    sendCommand("MPSS", payload);
}

void NetworkAtemBackend::captureStill(int64_t /*sourceInputId*/) {
    // The switcher captures its current program output
    sendCommand("Capt", QByteArray(4, 0));
}

void NetworkAtemBackend::setAuxSource(int auxIndex, int64_t inputId) {
    QByteArray payload(4, 0);
    payload[0] = 0x01; // mask: source
    payload[1] = static_cast<char>(auxIndex - 1);
    payload[2] = static_cast<char>((inputId >> 8) & 0xff);
    payload[3] = static_cast<char>(inputId & 0xff);
    sendCommand("CAuS", payload);
}

void NetworkAtemBackend::setConvertedVideoMode(int auxIndex, uint32_t videoMode) {
    unsupported("setConvertedVideoMode");
}

} // namespace AtemNet
