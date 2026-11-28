#pragma once

#include "OscTypes.h"
#include <QByteArray>
#include <QList>

namespace Osc {

class Packet {
public:
    static QByteArray encodeMessage(const Message &msg);
    static bool decode(const QByteArray &data, QList<Message> &outMessages);

};

} // namespace Osc
