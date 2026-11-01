#pragma once

#include "OscTypes.h"
#include <QByteArray>
#include <QList>

namespace Osc {

class Packet {
public:
    static QByteArray encodeMessage(const Message &msg);
    static bool decode(const QByteArray &data, QList<Message> &outMessages);

private:
    static bool decodeMessage(const char *data, int size, int &offset, Message &msg);
    static bool decodeBundle(const char *data, int size, int &offset, QList<Message> &outMessages, int depth);
    static QString readPaddedString(const char *data, int size, int &offset);
    static void appendPaddedString(QByteArray &buffer, const QString &str);
    static void appendInt32(QByteArray &buffer, int32_t val);
    static void appendFloat32(QByteArray &buffer, float val);
};

} // namespace Osc
