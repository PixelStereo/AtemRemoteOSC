#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include "OscTypes.h"

namespace Osc {

class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    ~Server() override;

    bool start(quint16 port, const QHostAddress &address = QHostAddress::Any);
    void stop();
    bool isListening() const;
    quint16 port() const { return m_port; }

signals:
    void messageReceived(const Osc::Message &message, const QHostAddress &sender, quint16 port);
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket = nullptr;
    quint16 m_port = 3333;
};

} // namespace Osc
