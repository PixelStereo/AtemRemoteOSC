#include "OscPacket.h"
#include <OscCodec.h>

namespace Osc {

QByteArray Packet::encodeMessage(const Message &msg)
{
    osc::Message wire;
    wire.address = msg.address();
    for (const auto &arg : msg.arguments()) {
        wire.types.append(QLatin1Char(static_cast<char>(arg.type())));
        switch (arg.type()) {
        case Type::Int32: wire.args.append(int(arg.toInt())); break;
        case Type::Float32: wire.args.append(arg.toFloat()); break;
        case Type::Int64: wire.args.append(qlonglong(arg.wireInt64())); break;
        case Type::Double: wire.args.append(arg.wireDouble()); break;
        case Type::String: wire.args.append(arg.toString()); break;
        case Type::Blob: wire.args.append(arg.toBlob()); break;
        default: wire.args.append(QVariant{}); break;
        }
    }
    return osc::encode(wire);
}

bool Packet::decode(const QByteArray &data, QList<Message> &outMessages)
{
    std::vector<osc::Message> decoded;
    if (!osc::decode(data, decoded)) return false;
    QList<Message> messages;
    for (const auto &wire : decoded) {
        Message msg(wire.address);
        for (qsizetype i = 0; i < wire.types.size(); ++i) {
            const auto &arg = wire.args.at(i);
            switch (wire.types.at(i).toLatin1()) {
            case 'i':
            case 'r': msg.addInt(static_cast<int32_t>(arg.toLongLong())); break;
            case 'f': msg.addFloat(arg.toFloat()); break;
            case 's':
            case 'S': msg.addString(arg.toString()); break;
            case 'b': msg.addBlob(arg.toByteArray()); break;
            case 'h': msg.addValue(Value(static_cast<int64_t>(arg.toLongLong()))); break;
            case 'd': msg.addValue(Value(arg.toDouble())); break;
            case 'T': msg.addBool(true); break;
            case 'F': msg.addBool(false); break;
            case 'N': msg.addNil(); break;
            case 'I': msg.addBang(); break;
            default: return false;
            }
        }
        messages.append(std::move(msg));
    }
    outMessages.append(messages);
    return true;
}

} // namespace Osc
