#include "OscServer.h"
#include "OscPacket.h"
#include <QNetworkDatagram>

namespace Osc {

Server::Server(QObject *parent)
    : QObject(parent)
{
}

Server::~Server() {
    stop();
}

bool Server::start(quint16 port, const QHostAddress &address) {
    stop();
    m_port = port;
    m_socket = new QUdpSocket(this);

    connect(m_socket, &QUdpSocket::readyRead, this, &Server::onReadyRead);

    if (!m_socket->bind(address, m_port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit errorOccurred(m_socket->errorString());
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    return true;
}

void Server::stop() {
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

bool Server::isListening() const {
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

void Server::onReadyRead() {
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        QList<Message> messages;
        if (Packet::decode(data, messages)) {
            for (const auto &msg : messages) {
                emit messageReceived(msg, datagram.senderAddress(), static_cast<quint16>(datagram.senderPort()));
            }
        }
    }
}

} // namespace Osc
