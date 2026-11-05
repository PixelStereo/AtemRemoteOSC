#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include "OscTypes.h"

namespace Osc {

class Client : public QObject {
    Q_OBJECT
public:
    explicit Client(QObject *parent = nullptr);
    ~Client() override;

    void setTarget(const QString &host, quint16 port);
    void sendMessage(const Message &msg);
    void sendMessage(const QString &address, const QVector<Value> &args);

    bool isValidTarget() const { return !m_targetAddress.isNull() && m_targetPort > 0; }
    QString targetHost() const { return m_targetAddress.toString(); }
    quint16 targetPort() const { return m_targetPort; }

signals:
    void errorOccurred(const QString &error);

private:
    QUdpSocket *m_socket = nullptr;
    QHostAddress m_targetAddress;
    quint16 m_targetPort = 0;
};

} // namespace Osc
