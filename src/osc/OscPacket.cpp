#include "OscPacket.h"
#include <QtEndian>
#include <cstring>

namespace Osc {

static inline int roundUp4(int n) {
    return (n + 3) & ~3;
}

constexpr int kMaxBundleDepth = 8;

template <typename T>
static inline T readBE(const char *p) {
    return qFromBigEndian<T>(reinterpret_cast<const uchar*>(p));
}

void Packet::appendPaddedString(QByteArray &buffer, const QString &str) {
    QByteArray utf8 = str.toUtf8();
    buffer.append(utf8);
    buffer.append('\0');
    int padding = 4 - (buffer.size() % 4);
    if (padding < 4) {
        buffer.append(padding, '\0');
    }
}

void Packet::appendInt32(QByteArray &buffer, int32_t val) {
    int32_t be = qToBigEndian(val);
    buffer.append(reinterpret_cast<const char*>(&be), sizeof(be));
}

void Packet::appendFloat32(QByteArray &buffer, float val) {
    static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
    uint32_t raw;
    std::memcpy(&raw, &val, sizeof(float));
    raw = qToBigEndian(raw);
    buffer.append(reinterpret_cast<const char*>(&raw), sizeof(raw));
}

QByteArray Packet::encodeMessage(const Message &msg) {
    QByteArray buffer;
    appendPaddedString(buffer, msg.address());

    // Build type tags
    QString tags = QStringLiteral(",");
    for (const auto &arg : msg.arguments()) {
        tags.append(static_cast<char>(arg.type()));
    }
    appendPaddedString(buffer, tags);

    // Append argument values
    for (const auto &arg : msg.arguments()) {
        switch (arg.type()) {
            case Type::Int32:
                appendInt32(buffer, arg.toInt());
                break;
            case Type::Float32:
                appendFloat32(buffer, arg.toFloat());
                break;
            case Type::String:
                appendPaddedString(buffer, arg.toString());
                break;
            case Type::Blob: {
                QByteArray b = arg.toBlob();
                appendInt32(buffer, b.size());
                buffer.append(b);
                int pad = 4 - (buffer.size() % 4);
                if (pad < 4) buffer.append(pad, '\0');
                break;
            }
            case Type::True:
            case Type::False:
            case Type::Nil:
            case Type::Bang:
                // No payload in OSC
                break;
            default:
                break;
        }
    }

    return buffer;
}

QString Packet::readPaddedString(const char *data, int size, int &offset) {
    if (offset >= size) return QString();

    int start = offset;
    while (offset < size && data[offset] != '\0') {
        offset++;
    }
    if (offset >= size) {
        offset = start;
        return QString();
    }

    QString result = QString::fromUtf8(data + start, offset - start);
    // Skip null terminator and 4-byte padding
    offset = roundUp4(offset + 1);
    if (offset > size) offset = size;
    return result;
}

bool Packet::decodeMessage(const char *data, int size, int &offset, Message &msg) {
    if (offset >= size) return false;

    QString address = readPaddedString(data, size, offset);
    if (address.isEmpty() || !address.startsWith('/')) {
        return false;
    }
    msg.setAddress(address);

    if (offset >= size) {
        return true; // No type tags, parameterless message
    }

    QString tags = readPaddedString(data, size, offset);
    if (!tags.startsWith(',')) {
        // According to OSC spec, if no type tag string, message has 0 arguments
        return true;
    }

    for (int i = 1; i < tags.size(); ++i) {
        char tag = tags[i].toLatin1();
        switch (tag) {
            case 'i':
            case 'r': {
                if (offset + 4 > size) return false;
                int32_t val = readBE<int32_t>(data + offset);
                offset += 4;
                msg.addInt(val);
                break;
            }
            case 'f': {
                if (offset + 4 > size) return false;
                uint32_t raw = readBE<uint32_t>(data + offset);
                offset += 4;
                float val;
                std::memcpy(&val, &raw, sizeof(float));
                msg.addFloat(val);
                break;
            }
            case 's':
            case 'S': {
                if (offset >= size) return false;
                int before = offset;
                QString s = readPaddedString(data, size, offset);
                if (offset == before) return false; // unterminated string
                msg.addString(s);
                break;
            }
            case 'b': {
                if (offset + 4 > size) return false;
                int32_t len = readBE<int32_t>(data + offset);
                offset += 4;
                if (len < 0 || len > size - offset) return false;
                QByteArray blob(data + offset, len);
                offset = qMin(size, offset + roundUp4(len));
                msg.addBlob(blob);
                break;
            }
            case 'T':
                msg.addBool(true);
                break;
            case 'F':
                msg.addBool(false);
                break;
            case 'N':
                msg.addNil();
                break;
            case 'I':
                msg.addBang();
                break;
            case 'h': {
                if (offset + 8 > size) return false;
                int64_t val = readBE<int64_t>(data + offset);
                offset += 8;
                msg.addValue(Value(val));
                break;
            }
            case 'd': {
                if (offset + 8 > size) return false;
                uint64_t raw = readBE<uint64_t>(data + offset);
                offset += 8;
                double val;
                std::memcpy(&val, &raw, sizeof(double));
                msg.addValue(Value(val));
                break;
            }
            default:
                // Unknown tag: payload size is unknown, so the rest cannot be parsed reliably
                return false;
        }
    }

    return true;
}

bool Packet::decodeBundle(const char *data, int size, int &offset, QList<Message> &outMessages, int depth) {
    if (depth > kMaxBundleDepth || offset + 16 > size) return false;
    // Bundle header: "#bundle\0" (8 bytes) + timetag (8 bytes)
    offset += 16;

    while (offset + 4 <= size) {
        int32_t elementSize = readBE<int32_t>(data + offset);
        offset += 4;
        if (elementSize <= 0 || offset + elementSize > size) {
            break;
        }

        int elementEnd = offset + elementSize;
        if (data[offset] == '#') {
            decodeBundle(data, elementEnd, offset, outMessages, depth + 1);
        } else if (data[offset] == '/') {
            Message m;
            if (decodeMessage(data, elementEnd, offset, m)) {
                outMessages.append(m);
            }
        }
        offset = elementEnd;
    }
    return true;
}

bool Packet::decode(const QByteArray &data, QList<Message> &outMessages) {
    if (data.isEmpty()) return false;
    int offset = 0;
    if (data.startsWith("#bundle")) {
        return decodeBundle(data.constData(), data.size(), offset, outMessages, 0);
    } else if (data.startsWith('/')) {
        Message msg;
        if (decodeMessage(data.constData(), data.size(), offset, msg)) {
            outMessages.append(msg);
            return true;
        }
    }
    return false;
}

} // namespace Osc
