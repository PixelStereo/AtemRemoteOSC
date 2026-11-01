#include "SwitcherListWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>

static QIcon createStatusIcon(Atem::ConnectionState state) {
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor color = QColor(160, 160, 160); // Disconnected
    if (state == Atem::ConnectionState::Connected) {
        color = QColor(46, 204, 113); // Green
    } else if (state == Atem::ConnectionState::Connecting) {
        color = QColor(243, 156, 18); // Orange
    } else if (state == Atem::ConnectionState::Failed) {
        color = QColor(231, 76, 60); // Red
    }

    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(1, 1, 10, 10);
    return QIcon(pixmap);
}

SwitcherListWidget::SwitcherListWidget(AtemOscApp *app, QWidget *parent)
    : QWidget(parent)
    , m_app(app)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(4, 4, 4, 4);
    btnLayout->setSpacing(4);

    m_btnAdd = new QPushButton(QStringLiteral("+"), this);
    m_btnAdd->setToolTip(QStringLiteral("Add Switcher"));
    m_btnAdd->setFixedWidth(32);
    btnLayout->addWidget(m_btnAdd);

    m_btnRemove = new QPushButton(QStringLiteral("−"), this);
    m_btnRemove->setToolTip(QStringLiteral("Remove Selected Switcher"));
    m_btnRemove->setFixedWidth(32);
    btnLayout->addWidget(m_btnRemove);

    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(m_btnAdd, &QPushButton::clicked, this, &SwitcherListWidget::onAddClicked);
    connect(m_btnRemove, &QPushButton::clicked, this, &SwitcherListWidget::onRemoveClicked);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &SwitcherListWidget::onItemSelectionChanged);

    connect(m_app, &AtemOscApp::switcherListChanged, this, &SwitcherListWidget::refreshList);
    connect(m_app, &AtemOscApp::switcherUpdated, this, &SwitcherListWidget::refreshList);

    refreshList();
}

int SwitcherListWidget::selectedIndex() const {
    return m_list->currentRow();
}

void SwitcherListWidget::refreshList() {
    int current = m_list->currentRow();
    m_list->clear();

    const auto &switchers = m_app->switchers();
    for (int i = 0; i < switchers.size(); ++i) {
        auto *sw = switchers[i];
        QString title = sw->backend ? sw->backend->switcherName() : QStringLiteral("ATEM Switcher");
        if (!sw->config.nickname.isEmpty()) {
            title += QStringLiteral(" (\"%1\")").arg(sw->config.nickname);
        }
        QString subtitle = QStringLiteral("%1").arg(sw->config.ip);

        auto *item = new QListWidgetItem(createStatusIcon(sw->backend ? sw->backend->connectionState() : Atem::ConnectionState::Disconnected),
                                         QStringLiteral("%1\n%2").arg(title, subtitle), m_list);
        item->setData(Qt::UserRole, i);
    }

    if (current >= 0 && current < m_list->count()) {
        m_list->setCurrentRow(current);
    } else if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
}

void SwitcherListWidget::onAddClicked() {
    SwitcherConfig cfg;
    cfg.ip = QStringLiteral("192.168.10.240");
    cfg.outgoingIp = QStringLiteral("127.0.0.1");
    cfg.outgoingPort = 53000;
    m_app->addSwitcher(cfg);
    m_list->setCurrentRow(m_list->count() - 1);
}

void SwitcherListWidget::onRemoveClicked() {
    int row = m_list->currentRow();
    if (row >= 0) {
        m_app->removeSwitcher(row);
    }
}

void SwitcherListWidget::onItemSelectionChanged() {
    emit selectionChanged(m_list->currentRow());
}
