#include "SwitcherListWidget.h"
#include "Theme.h"
#include <QPainter>
#include <QPixmap>

namespace {
QIcon dot(const QColor &c)
{
    QPixmap pm(20, 20);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QPointF(5, 5), 4, 4);
    return QIcon(pm);
}

QColor stateColor(Atem::ConnectionState state)
{
    switch (state) {
        case Atem::ConnectionState::Connected: return theme::okColor();
        case Atem::ConnectionState::Connecting: return theme::warnColor();
        case Atem::ConnectionState::Failed: return theme::errorColor();
        default: return theme::mutedColor();
    }
}
} // namespace

SwitcherListWidget::SwitcherListWidget(AtemRemoteOscApp *app, QWidget *parent)
    : QWidget(parent)
    , m_app(app)
{
    m_group.setExclusive(true);
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    m_chips = new QHBoxLayout;
    m_chips->setSpacing(8);
    row->addLayout(m_chips);

    m_btnAdd = new QPushButton(QStringLiteral("+ Add"), this);
    m_btnAdd->setObjectName(QStringLiteral("chip"));
    m_btnAdd->setToolTip(QStringLiteral("Add a switcher; nothing connects until you press Connect"));
    m_btnRemove = new QPushButton(QStringLiteral("Remove"), this);
    m_btnRemove->setObjectName(QStringLiteral("chip"));
    m_btnRemove->setToolTip(QStringLiteral("Remove the selected switcher"));
    row->addWidget(m_btnAdd);
    row->addWidget(m_btnRemove);
    row->addStretch();

    connect(m_btnAdd, &QPushButton::clicked, this, &SwitcherListWidget::addSwitcher);
    connect(m_btnRemove, &QPushButton::clicked, this, [this] {
        if (m_selected >= 0) m_app->removeSwitcher(m_selected);
    });
    connect(m_app, &AtemRemoteOscApp::switcherListChanged, this, &SwitcherListWidget::refreshList);
    connect(m_app, &AtemRemoteOscApp::switcherUpdated, this, &SwitcherListWidget::refreshList);
    refreshList();
}

void SwitcherListWidget::select(int index)
{
    if (index == m_selected) return;
    m_selected = index;
    emit selectionChanged(index);
}

void SwitcherListWidget::refreshList()
{
    // Rebuilding the chips is not a selection change: without this, every connection state change would
    // reload the editable fields and overwrite what is being typed
    const int previous = m_selected;
    for (auto *b : m_buttons) {
        m_group.removeButton(b);
        m_chips->removeWidget(b);
        b->deleteLater();
    }
    m_buttons.clear();

    const auto &switchers = m_app->switchers();
    for (int i = 0; i < switchers.size(); ++i) {
        auto *sw = switchers[i];
        const auto state = sw->backend ? sw->backend->connectionState() : Atem::ConnectionState::Disconnected;
        QString name = sw->config.nickname.isEmpty() ? QStringLiteral("Switcher %1").arg(i + 1) : sw->config.nickname;
        if (sw->backend && state == Atem::ConnectionState::Connected && !sw->backend->switcherName().isEmpty())
            name = sw->backend->switcherName();
        const QString address = sw->config.ip.isEmpty() ? QStringLiteral("no address") : sw->config.ip;
        auto *b = new QPushButton(QStringLiteral("%1   %2").arg(name, address), this);
        b->setObjectName(QStringLiteral("chip"));
        b->setCheckable(true);
        b->setIcon(dot(stateColor(state)));
        b->setIconSize(QSize(10, 10));
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumWidth(b->fontMetrics().horizontalAdvance(b->text()) + 70);
        m_group.addButton(b, i);
        m_chips->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i] { select(i); });
        m_buttons.append(b);
    }

    int target = previous;
    if (target < 0 || target >= switchers.size()) target = switchers.isEmpty() ? -1 : (previous >= switchers.size() ? switchers.size() - 1 : 0);
    if (target >= 0) m_buttons[target]->setChecked(true);
    m_btnRemove->setEnabled(!switchers.isEmpty());
    // Same switcher as before: no signal. A different one (first one, or the one removed): the editor must follow.
    if (target != previous) {
        m_selected = target;
        emit selectionChanged(target);
    }
}

void SwitcherListWidget::addSwitcher()
{
    SwitcherConfig cfg;
    cfg.ip.clear(); // to be filled in by the user
    cfg.outgoingIp = QStringLiteral("127.0.0.1");
    cfg.outgoingPort = 53000;
    m_app->addSwitcher(cfg);
    const int last = m_app->switchers().size() - 1;
    if (last >= 0 && last < m_buttons.size()) {
        m_buttons[last]->setChecked(true);
        select(last);
    }
}
