#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

namespace AtemNet {

// Flags live in the top 5 bits of the first header byte; the low 3 bits are the length's high bits.
enum PacketFlags : uint8_t {
    FlagAckRequest        = 0x01,
    FlagNewSession        = 0x02,
    FlagRetransmit        = 0x04,
    FlagRetransmitRequest = 0x08,
    FlagAckReply          = 0x10
};

struct PacketHeader {
    uint8_t flags = 0;
    uint16_t length = 0;
    uint16_t sessionId = 0;
    uint16_t ackedPacketId = 0;  // bytes 4-5: id of the packet being acknowledged
    uint16_t packetId = 0;       // bytes 10-11: sequence number of this packet
};

class Packet {
public:
    static QByteArray createHello(uint16_t uid = 0x53ab);
    static QByteArray createAck(uint16_t sessionId, uint16_t ackedPacketId);
    // Increments `localPacketId` and stamps the new value into the packet.
    static QByteArray createCommandPacket(uint16_t sessionId, uint16_t &localPacketId,
                                          const QByteArray &commandPayload);

    static bool parseHeader(const QByteArray &data, PacketHeader &header);
};

} // namespace AtemNet
