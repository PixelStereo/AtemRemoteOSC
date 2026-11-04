#include "OscQueryServer.h"
#include <QJsonDocument>
#include <QUrl>

namespace Osc {

namespace {
constexpr int kMaxRequestBytes = 16 * 1024;

QByteArray statusLine(int status)
{
    switch (status) {
        case 200: return "200 OK";
        case 204: return "204 No Content";
        case 400: return "400 Bad Request";
        case 404: return "404 Not Found";
        case 405: return "405 Method Not Allowed";
        default: return QByteArray::number(status);
    }
}
} // namespace

QueryServer::QueryServer(QueryTree *tree, QObject *parent)
    : QObject(parent)
    , m_tree(tree)
{
    connect(&m_server, &QTcpServer::newConnection, this, &QueryServer::onNewConnection);
}

bool QueryServer::start(quint16 port)
{
    stop();
    if (!m_server.listen(QHostAddress::Any, port)) {
        emit logMessage(QStringLiteral("OSCQuery: cannot listen on TCP port %1: %2").arg(port).arg(m_server.errorString()));
        return false;
    }
    emit logMessage(QStringLiteral("OSCQuery listening on HTTP port %1").arg(m_server.serverPort()));
    return true;
}

void QueryServer::stop()
{
    if (m_server.isListening()) m_server.close();
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) it.key()->disconnectFromHost();
    m_buffers.clear();
}

void QueryServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void QueryServer::onReadyRead(QTcpSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();
    if (buffer.size() > kMaxRequestBytes) {
        socket->disconnectFromHost();
        return;
    }
    const int end = buffer.indexOf("\r\n\r\n");
    if (end < 0) return; // headers not complete yet

    const QList<QByteArray> requestLine = buffer.left(buffer.indexOf("\r\n")).split(' ');
    Response resp;
    if (requestLine.size() < 2) {
        resp.status = 400;
    } else {
        resp = answer(QString::fromLatin1(requestLine[0]), requestLine[1]);
    }
    buffer.clear();

    QByteArray out = "HTTP/1.1 " + statusLine(resp.status) + "\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                     "Connection: close\r\n";
    if (!resp.body.isEmpty()) {
        out += "Content-Type: application/json; charset=utf-8\r\n";
    }
    out += "Content-Length: " + QByteArray::number(resp.body.size()) + "\r\n\r\n";
    if (requestLine.isEmpty() || requestLine[0] != "HEAD") out += resp.body;
    socket->write(out);
    socket->disconnectFromHost();
}

QueryServer::Response QueryServer::answer(const QString &method, const QByteArray &target)
{
    Response resp;
    if (method == QLatin1String("OPTIONS")) {
        resp.status = 204;
        return resp;
    }
    if (method != QLatin1String("GET") && method != QLatin1String("HEAD")) {
        resp.status = 405;
        return resp;
    }

    const QUrl url = QUrl::fromEncoded(target, QUrl::TolerantMode);
    if (!url.isValid()) {
        resp.status = 400;
        return resp;
    }
    QString path = url.path();
    if (path.isEmpty()) path = QStringLiteral("/");
    const QString query = url.query().toUpper();

    QJsonObject json;
    if (query == QLatin1String("HOST_INFO")) {
        json = m_tree->hostInfo(m_name, m_oscPort);
    } else if (!query.isEmpty()) {
        if (!m_tree->attribute(path, query, json)) {
            resp.status = 404;
            return resp;
        }
        if (json.isEmpty()) {
            resp.status = 204; // the attribute has no value
            return resp;
        }
    } else if (!m_tree->node(path, json)) {
        resp.status = 404;
        return resp;
    }
    resp.body = QJsonDocument(json).toJson(QJsonDocument::Compact);
    return resp;
}

} // namespace Osc
