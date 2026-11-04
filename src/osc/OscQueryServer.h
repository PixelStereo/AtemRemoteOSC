#pragma once

#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include "OscQueryTree.h"

namespace Osc {

// Minimal HTTP server for OSCQuery: GET <osc path>[?VALUE|?TYPE|...] and GET /?HOST_INFO, answered in JSON.
class QueryServer : public QObject {
    Q_OBJECT
public:
    QueryServer(QueryTree *tree, QObject *parent = nullptr);

    bool start(quint16 port);
    void stop();
    bool isListening() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }

    void setName(const QString &name) { m_name = name; }
    void setOscPort(quint16 port) { m_oscPort = port; }

    // The HTTP answer to a request line target, split out for tests
    struct Response {
        int status = 200;
        QByteArray body;
    };
    Response answer(const QString &method, const QByteArray &target);

signals:
    void logMessage(const QString &text);

private slots:
    void onNewConnection();

private:
    void onReadyRead(QTcpSocket *socket);

    QueryTree *m_tree;
    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    QString m_name = QStringLiteral("AtemRemoteOsc");
    quint16 m_oscPort = 3333;
};

} // namespace Osc
