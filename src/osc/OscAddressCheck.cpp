#include "OscAddressCheck.h"
#include "OscDispatcher.h"
#include "OscPacket.h"
#include "OscQueryTree.h"
#include "../backend/null/NullAtemBackend.h"

#include <QEventLoop>
#include <QTimer>
#include <cstdio>

namespace Osc {

int checkDeclaredAddresses()
{
    AddressRegistry registry;
    NullAtemBackend backend;
    Dispatcher dispatcher(&registry);
    dispatcher.addSwitcher({}, &backend, nullptr);
    Server server;
    if (!server.start(0, QHostAddress::LocalHost)) {
        std::fputs("Could not bind a temporary OSC loopback port.\n", stderr);
        return 1;
    }
    QObject::connect(&server, &Server::messageReceived, &dispatcher, &Dispatcher::dispatchMessage);
    QEventLoop wait;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &wait, &QEventLoop::quit);
    QString received;
    QObject::connect(&server, &Server::messageReceived, &wait,
                     [&](const Message &msg, const QHostAddress &, quint16) {
                         received = msg.address();
                         wait.quit();
                     });
    QString rejection;
    QObject::connect(&dispatcher, &Dispatcher::logMessage, &wait, [&](const QString &line) {
        if (line.startsWith("Unknown or unsupported OSC address:") || line.startsWith("Read-only OSC address:") ||
            line.startsWith("Error:") || line.contains("is not implemented")) rejection = line;
    });

    QueryTree query(&registry, &dispatcher);
    query.refresh();
    const QueryTree::Counts counts{backend.meCount(), backend.dskCount(), backend.auxCount(),
                                   backend.hyperdeckCount(), int(backend.maxMacros()), {}};
    QUdpSocket sender;
    QStringList failures;
    int attempted = 0;
    int readOnly = 0;
    for (const auto &ep : registry.endpoints()) {
        for (const auto &path : QueryTree::expand(ep.pattern, counts)) {
            Message message("/atem" + path);
            if (ep.valueType == "int") message.addInt(ep.hasRange ? int(ep.rangeMax) : 1);
            else if (ep.valueType == "float") message.addFloat(ep.hasRange ? float(ep.rangeMax) : 0.5f);
            else if (ep.valueType == "bool") message.addBool(true);
            else if (ep.valueType == "string") {
                if (!ep.choices.isEmpty()) message.addString(ep.choices.front());
                else if (ep.pattern == "/clock/time") message.addString("00:00:01");
                else if (ep.pattern.startsWith("/hyperdeck/")) message.addString("00:00:00:01");
                else message.addString("smoke-test");
            } else if (ep.valueType == "none") {
                message.addBang();
            } else {
                failures << ep.pattern + ": no test value for " + ep.valueType;
                continue;
            }
            QJsonObject node;
            if (!query.node(message.address(), node) || ((node.value("ACCESS").toInt() & 2) != 0) != ep.writable)
                failures << ep.pattern + ": OSCQuery access does not match the registry";

            const auto packet = Packet::encodeMessage(message);
            if (packet.isEmpty() || sender.writeDatagram(packet, QHostAddress::LocalHost, server.port()) != packet.size()) {
                std::fprintf(stderr, "Could not send OSC probe for %s.\n", qPrintable(message.address()));
                return 1;
            }
            ++attempted;
            if (!ep.writable) ++readOnly;
            received.clear();
            rejection.clear();
            timeout.start(1000);
            wait.exec();
            timeout.stop();
            if (received != message.address()) {
                std::fprintf(stderr, "No matching OSC datagram received for %s.\n", qPrintable(message.address()));
                return 1;
            }
            if (ep.writable ? !rejection.isEmpty() : !rejection.startsWith("Read-only OSC address:"))
                failures << ep.pattern + (rejection.isEmpty() ? ": read-only address accepted a write" : ": command rejected");
        }
    }
    failures.removeDuplicates();
    for (const auto &failure : failures) std::fprintf(stderr, "%s\n", qPrintable(failure));
    std::printf("OSC datagrams tested: %d | writable: %d | read-only: %d\n", attempted, attempted - readOnly, readOnly);
    return failures.isEmpty() && attempted > 0 ? 0 : 1;
}

} // namespace Osc
