#include "NetworkPanel.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QStringList localAddresses()
{
    QStringList out;
    for (const QNetworkInterface &nic : QNetworkInterface::allInterfaces()) {
        if (!(nic.flags() & QNetworkInterface::IsUp) || !(nic.flags() & QNetworkInterface::IsRunning)) continue;
        if (nic.flags() & QNetworkInterface::IsLoopBack) continue;
        for (const QNetworkAddressEntry &e : nic.addressEntries())
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) out << e.ip().toString();
    }
    return out;
}
} // namespace

NetworkPanel::NetworkPanel(AtemRemoteOscApp *app, QWidget *parent)
    : QWidget(parent)
    , m_app(app)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    // OSC input
    auto *grpIn = new QGroupBox(QStringLiteral("OSC input"), this);
    grpIn->setObjectName(QStringLiteral("section"));
    auto *formIn = new QFormLayout(grpIn);
    formIn->setLabelAlignment(Qt::AlignRight);
    auto *portRow = new QHBoxLayout;
    m_spnPort = new QSpinBox(grpIn);
    m_spnPort->setRange(1024, 65535);
    m_spnPort->setValue(m_app->incomingPort());
    m_btnApply = new QPushButton(QStringLiteral("Apply"), grpIn);
    m_lblOsc = new QLabel(grpIn);
    m_lblOsc->setObjectName(QStringLiteral("pill"));
    portRow->addWidget(m_spnPort);
    portRow->addWidget(m_btnApply);
    portRow->addWidget(m_lblOsc);
    portRow->addStretch();
    formIn->addRow(QStringLiteral("UDP port"), portRow);
    const QStringList addresses = localAddresses();
    auto *where = new QLabel(addresses.isEmpty()
                                 ? QStringLiteral("No network address found.")
                                 : QStringLiteral("Send OSC to %1, port %2").arg(addresses.join(QStringLiteral(" or ")))
                                       .arg(m_app->incomingPort()), grpIn);
    where->setObjectName(QStringLiteral("hint"));
    where->setTextInteractionFlags(Qt::TextSelectableByMouse);
    formIn->addRow(QString(), where);
    layout->addWidget(grpIn);

    // OSCQuery
    auto *grpQuery = new QGroupBox(QStringLiteral("OSCQuery"), this);
    grpQuery->setObjectName(QStringLiteral("section"));
    auto *queryLayout = new QVBoxLayout(grpQuery);
    auto *queryRow = new QHBoxLayout;
    m_chkQuery = new QCheckBox(QStringLiteral("Publish the OSC addresses"), grpQuery);
    m_chkQuery->setChecked(m_app->oscQueryEnabled());
    m_spnQueryPort = new QSpinBox(grpQuery);
    m_spnQueryPort->setRange(1024, 65535);
    m_spnQueryPort->setValue(m_app->oscQueryPort());
    m_spnQueryPort->setPrefix(QStringLiteral("HTTP "));
    m_lblQuery = new QLabel(grpQuery);
    m_lblQuery->setObjectName(QStringLiteral("pill"));
    queryRow->addWidget(m_chkQuery);
    queryRow->addWidget(m_spnQueryPort);
    queryRow->addWidget(m_lblQuery);
    queryRow->addStretch();
    queryLayout->addLayout(queryRow);
    auto *queryHint = new QLabel(QStringLiteral("OSCQuery clients find AtemRemoteOsc on the network by themselves, "
                                                "and get every address with its range and its current value."), grpQuery);
    queryHint->setObjectName(QStringLiteral("hint"));
    queryHint->setWordWrap(true);
    queryLayout->addWidget(queryHint);
    layout->addWidget(grpQuery);
    layout->addStretch();

    connect(m_btnApply, &QPushButton::clicked, this, &NetworkPanel::applyPort);
    connect(m_chkQuery, &QCheckBox::toggled, this, &NetworkPanel::applyOscQuery);
    connect(m_spnQueryPort, &QSpinBox::editingFinished, this, &NetworkPanel::applyOscQuery);
    connect(m_app, &AtemRemoteOscApp::oscServerStateChanged, this, [this](bool, quint16) { updateOscStatus(); });
    connect(m_app, &AtemRemoteOscApp::oscQueryStateChanged, this, [this](bool, quint16) { updateQueryStatus(); });
    updateOscStatus();
    updateQueryStatus();
}

void NetworkPanel::setPill(QLabel *label, const QString &text, const char *state)
{
    label->setText(text);
    label->setProperty("state", QString::fromLatin1(state));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

void NetworkPanel::updateOscStatus()
{
    if (m_app->isOscServerListening()) setPill(m_lblOsc, QStringLiteral("Listening"), "ok");
    else setPill(m_lblOsc, QStringLiteral("Not listening"), "bad");
}

void NetworkPanel::updateQueryStatus()
{
    if (m_app->isOscQueryRunning()) setPill(m_lblQuery, QStringLiteral("Running"), "ok");
    else if (m_app->oscQueryEnabled()) setPill(m_lblQuery, QStringLiteral("Port busy?"), "bad");
    else setPill(m_lblQuery, QStringLiteral("Off"), "");
}

void NetworkPanel::applyPort() { m_app->setIncomingPort(static_cast<quint16>(m_spnPort->value())); }

void NetworkPanel::applyOscQuery()
{
    m_app->setOscQuery(m_chkQuery->isChecked(), static_cast<quint16>(m_spnQueryPort->value()));
}
