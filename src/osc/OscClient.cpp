#include "OscClient.h"
#include "OscPacket.h"

namespace Osc {

Client::Client(QObject *parent)
    : QObject(parent)
{
    m_socket = new QUdpSocket(this);
}

Client::~Client() {
    if (m_socket) {
        m_socket->close();
    }
}

void Client::setTarget(const QString &host, quint16 port) {
    m_targetAddress.setAddress(host);
    m_targetPort = port;
}

void Client::sendMessage(const Message &msg) {
    if (!isValidTarget() || !m_socket) {
        return;
    }
    QByteArray data = Packet::encodeMessage(msg);
    qint64 written = m_socket->writeDatagram(data, m_targetAddress, m_targetPort);
    if (written < 0) {
        emit errorOccurred(m_socket->errorString());
    }
}

void Client::sendMessage(const QString &address, const QVector<Value> &args) {
    Message msg(address);
    for (const auto &v : args) {
        msg.addValue(v);
    }
    sendMessage(msg);
}

} // namespace Osc
