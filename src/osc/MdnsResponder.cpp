#include "MdnsResponder.h"
#include <QDateTime>
#include <QHostInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QtEndian>

namespace Osc {

namespace {

const QHostAddress kGroup(QStringLiteral("224.0.0.251"));
constexpr quint16 kMdnsPort = 5353;
constexpr quint16 TypeA = 1, TypePTR = 12, TypeTXT = 16, TypeSRV = 33, TypeANY = 255;
constexpr quint16 ClassIN = 1, CacheFlush = 0x8000;

void put16(QByteArray &b, quint16 v)
{
    b.append(char(v >> 8));
    b.append(char(v & 0xff));
}

void put32(QByteArray &b, quint32 v)
{
    put16(b, quint16(v >> 16));
    put16(b, quint16(v & 0xffff));
}

// name, type, class, ttl, then the data with its length
void putRecord(QByteArray &b, const QString &name, quint16 type, quint16 cls, quint32 ttl, const QByteArray &data)
{
    b += MdnsResponder::encodeName(name);
    put16(b, type);
    put16(b, cls);
    put32(b, ttl);
    put16(b, quint16(data.size()));
    b += data;
}

QString hostLabel()
{
    QString h = QHostInfo::localHostName().toLower();
    if (h.endsWith(QLatin1String(".local"))) h.chop(6);
    h.remove(QRegularExpression(QStringLiteral("[^a-z0-9-]")));
    if (h.isEmpty()) h = QStringLiteral("host");
    return QStringLiteral("atemremoteosc-") + h.left(40);
}

} // namespace

MdnsResponder::MdnsResponder(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &MdnsResponder::onReadyRead);
}

MdnsResponder::~MdnsResponder()
{
    stop();
}

QByteArray MdnsResponder::encodeName(const QString &dottedName)
{
    QByteArray out;
    for (const QString &label : dottedName.split('.', Qt::SkipEmptyParts)) {
        const QByteArray utf8 = label.toUtf8().left(63);
        out.append(char(utf8.size()));
        out += utf8;
    }
    out.append('\0');
    return out;
}

QByteArray MdnsResponder::buildResponse(const QString &instance, const QString &host, const QVector<Service> &services,
                                        const QList<QHostAddress> &addresses, quint32 ttl)
{
    QByteArray records;
    int count = 0;
    const QString hostName = host + QStringLiteral(".local");

    for (const Service &s : services) {
        const QString type = s.type + QStringLiteral(".local");
        const QString full = instance + QLatin1Char('.') + type;

        putRecord(records, type, TypePTR, ClassIN, ttl, encodeName(full)); // service type -> instance
        ++count;

        QByteArray srv;
        put16(srv, 0); // priority
        put16(srv, 0); // weight
        put16(srv, s.port);
        srv += encodeName(hostName);
        putRecord(records, full, TypeSRV, ClassIN | CacheFlush, ttl, srv);
        ++count;

        putRecord(records, full, TypeTXT, ClassIN | CacheFlush, ttl, QByteArray(1, '\0')); // empty TXT
        ++count;
    }
    for (const QHostAddress &a : addresses) {
        if (a.protocol() != QAbstractSocket::IPv4Protocol) continue;
        QByteArray ip;
        put32(ip, a.toIPv4Address());
        putRecord(records, hostName, TypeA, ClassIN | CacheFlush, ttl, ip);
        ++count;
    }

    QByteArray packet;
    put16(packet, 0);      // id
    put16(packet, 0x8400); // response, authoritative
    put16(packet, 0);      // questions
    put16(packet, quint16(count));
    put16(packet, 0);
    put16(packet, 0);
    return packet + records;
}

QStringList MdnsResponder::parseQuestions(const QByteArray &p)
{
    QStringList names;
    if (p.size() < 12) return names;
    const quint16 flags = qFromBigEndian<quint16>(p.constData() + 2);
    if (flags & 0x8000) return names; // a response
    const quint16 questions = qFromBigEndian<quint16>(p.constData() + 4);

    int pos = 12;
    for (int q = 0; q < questions; ++q) {
        QStringList labels;
        int jumps = 0, cursor = pos;
        bool jumped = false;
        while (true) {
            if (cursor >= p.size()) return QStringList();
            const quint8 len = quint8(p[cursor]);
            if (len == 0) { ++cursor; break; }
            if ((len & 0xC0) == 0xC0) { // compression pointer
                if (cursor + 1 >= p.size() || ++jumps > 8) return QStringList();
                const int target = ((len & 0x3F) << 8) | quint8(p[cursor + 1]);
                if (!jumped) { pos = cursor + 2; jumped = true; }
                cursor = target;
                continue;
            }
            if (cursor + 1 + len > p.size()) return QStringList();
            labels << QString::fromUtf8(p.constData() + cursor + 1, len);
            cursor += 1 + len;
        }
        if (!jumped) pos = cursor;
        pos += 4; // type, class
        if (pos > p.size()) return QStringList();
        names << labels.join('.').toLower();
    }
    return names;
}

QList<QHostAddress> MdnsResponder::localAddresses() const
{
    QList<QHostAddress> out;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack)) continue;
        for (const auto &entry : iface.addressEntries()) {
            const QHostAddress a = entry.ip();
            if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback()) out << a;
        }
    }
    return out;
}

bool MdnsResponder::start(const QString &instanceName, const QVector<Service> &services)
{
    stop();
    m_instance = instanceName;
    m_services = services;
    m_host = hostLabel();

    if (!m_socket.bind(QHostAddress::AnyIPv4, kMdnsPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit logMessage(QStringLiteral("OSCQuery: cannot announce on the network (mDNS): %1").arg(m_socket.errorString()));
        return false;
    }
    m_socket.setSocketOption(QAbstractSocket::MulticastTtlOption, 255);
    m_socket.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);

    m_interfaces.clear();
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || !(flags & QNetworkInterface::CanMulticast)) continue;
        if (m_socket.joinMulticastGroup(kGroup, iface)) m_interfaces << iface;
    }
    if (m_interfaces.isEmpty() && !m_socket.joinMulticastGroup(kGroup)) {
        emit logMessage(QStringLiteral("OSCQuery: cannot join the mDNS multicast group: %1").arg(m_socket.errorString()));
        m_socket.close();
        return false;
    }

    m_running = true;
    announce(120);
    QTimer::singleShot(1000, this, [this]() { if (m_running) announce(120); });
    emit logMessage(QStringLiteral("OSCQuery announced on the network as \"%1\"").arg(m_instance));
    return true;
}

void MdnsResponder::stop()
{
    if (!m_running) return;
    announce(0); // goodbye: the records are dropped from the caches
    m_running = false;
    m_socket.close();
    m_interfaces.clear();
}

void MdnsResponder::announce(quint32 ttl)
{
    send(buildResponse(m_instance, m_host, m_services, localAddresses(), ttl));
}

void MdnsResponder::send(const QByteArray &packet)
{
    if (m_interfaces.isEmpty()) {
        m_socket.writeDatagram(packet, kGroup, kMdnsPort);
        return;
    }
    for (const QNetworkInterface &iface : m_interfaces) {
        m_socket.setMulticastInterface(iface);
        m_socket.writeDatagram(packet, kGroup, kMdnsPort);
    }
}

void MdnsResponder::onReadyRead()
{
    while (m_socket.hasPendingDatagrams()) {
        QByteArray data;
        data.resize(int(m_socket.pendingDatagramSize()));
        m_socket.readDatagram(data.data(), data.size());

        const QStringList asked = parseQuestions(data);
        if (asked.isEmpty() || !m_running) continue;

        const QString host = m_host + QStringLiteral(".local");
        bool relevant = false;
        for (const QString &name : asked) {
            if (name == host) relevant = true;
            for (const Service &s : m_services) {
                const QString type = s.type + QStringLiteral(".local");
                if (name == type || name == (m_instance + QLatin1Char('.') + type).toLower()) relevant = true;
            }
        }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (relevant && now - m_lastAnswerMs > 250) { // do not answer a burst of identical queries one by one
            m_lastAnswerMs = now;
            announce(120);
        }
    }
}

} // namespace Osc
