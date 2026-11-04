#pragma once

#include <QHostAddress>
#include <QList>
#include <QNetworkInterface>
#include <QObject>
#include <QUdpSocket>
#include <QVector>

namespace Osc {

// Announces services on the local network (multicast DNS / DNS-SD), so that OSCQuery clients find the
// application without typing an address: "_oscjson._tcp" for the HTTP side, "_osc._udp" for the OSC port.
class MdnsResponder : public QObject {
    Q_OBJECT
public:
    struct Service {
        QString type;   // e.g. "_oscjson._tcp"
        quint16 port = 0;
    };

    explicit MdnsResponder(QObject *parent = nullptr);
    ~MdnsResponder() override;

    // Joins the multicast group and announces; false (with a log message) if the port cannot be used
    bool start(const QString &instanceName, const QVector<Service> &services);
    void stop(); // says goodbye
    bool isRunning() const { return m_running; }

    // Wire format, public for tests
    static QByteArray buildResponse(const QString &instance, const QString &host, const QVector<Service> &services,
                                    const QList<QHostAddress> &addresses, quint32 ttl);
    // The names asked in a DNS query packet (empty for responses and malformed packets)
    static QStringList parseQuestions(const QByteArray &packet);
    static QByteArray encodeName(const QString &dottedName);

signals:
    void logMessage(const QString &text);

private slots:
    void onReadyRead();

private:
    void announce(quint32 ttl);
    void send(const QByteArray &packet);
    QList<QHostAddress> localAddresses() const;

    QUdpSocket m_socket;
    QList<QNetworkInterface> m_interfaces; // those the group was joined on
    QString m_instance;
    QString m_host;
    QVector<Service> m_services;
    bool m_running = false;
    qint64 m_lastAnswerMs = 0;
};

} // namespace Osc
