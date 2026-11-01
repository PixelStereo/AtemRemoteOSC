#include "AtemPacket.h"

namespace AtemNet {

static void putHeaderStart(QByteArray &data, uint8_t flags, uint16_t length, uint16_t sessionId) {
    data[0] = static_cast<char>((flags << 3) | ((length >> 8) & 0x07));
    data[1] = static_cast<char>(length & 0xff);
    data[2] = static_cast<char>((sessionId >> 8) & 0xff);
    data[3] = static_cast<char>(sessionId & 0xff);
}

QByteArray Packet::createHello(uint16_t uid) {
    QByteArray data(20, 0);
    putHeaderStart(data, FlagNewSession, 20, uid);
    data[9] = 0x3a;
    data[12] = 0x01;
    return data;
}

QByteArray Packet::createAck(uint16_t sessionId, uint16_t ackedPacketId) {
    QByteArray data(12, 0);
    putHeaderStart(data, FlagAckReply, 12, sessionId);
    data[4] = static_cast<char>((ackedPacketId >> 8) & 0xff);
    data[5] = static_cast<char>(ackedPacketId & 0xff);
    return data;
}

QByteArray Packet::createCommandPacket(uint16_t sessionId, uint16_t &localPacketId,
                                      const QByteArray &commandPayload) {
    const uint16_t totalLen = static_cast<uint16_t>(12 + commandPayload.size());
    localPacketId = (localPacketId + 1) & 0x7fff;

    QByteArray data(12, 0);
    putHeaderStart(data, FlagAckRequest, totalLen, sessionId);
    data[10] = static_cast<char>((localPacketId >> 8) & 0xff);
    data[11] = static_cast<char>(localPacketId & 0xff);
    data.append(commandPayload);
    return data;
}

bool Packet::parseHeader(const QByteArray &data, PacketHeader &header) {
    if (data.size() < 12) return false;

    const uint8_t *bytes = reinterpret_cast<const uint8_t*>(data.constData());
    header.flags = (bytes[0] >> 3) & 0x1f;
    header.length = ((bytes[0] & 0x07) << 8) | bytes[1];
    header.sessionId = (bytes[2] << 8) | bytes[3];
    header.ackedPacketId = (bytes[4] << 8) | bytes[5];
    header.packetId = (bytes[10] << 8) | bytes[11];
    return header.length >= 12 && header.length <= data.size();
}

} // namespace AtemNet
