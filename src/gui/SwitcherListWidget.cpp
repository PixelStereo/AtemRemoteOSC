#include "SwitcherListWidget.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {

enum Roles { TitleRole = Qt::UserRole + 1, AddressRole, StateRole, StateTextRole };

QColor stateColor(Atem::ConnectionState state)
{
    switch (state) {
        case Atem::ConnectionState::Connected: return theme::okColor();
        case Atem::ConnectionState::Connecting: return theme::warnColor();
        case Atem::ConnectionState::Failed: return theme::errorColor();
        default: return theme::mutedColor();
    }
}

class CardDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return QSize(100, 62); }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF card = QRectF(option.rect).adjusted(2, 3, -2, -3);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hover = option.state & QStyle::State_MouseOver;
        p->setPen(QPen(selected ? theme::accent() : QColor(0x62, 0x62, 0x6e), selected ? 1.4 : 1));
        QColor fill = QColor(0x2f, 0x2f, 0x34);
        if (hover && !selected) fill = QColor(0x38, 0x38, 0x3e);
        if (selected) fill = QColor(0x3a, 0x3a, 0x42);
        p->setBrush(fill);
        p->drawRoundedRect(card, 6, 6);

        const auto state = static_cast<Atem::ConnectionState>(index.data(StateRole).toInt());
        p->setPen(Qt::NoPen);
        p->setBrush(stateColor(state));
        p->drawEllipse(QPointF(card.left() + 16, card.center().y()), 5, 5);

        const QRectF text = card.adjusted(30, 8, -10, -8);
        QFont bold = option.font;
        bold.setBold(true);
        p->setFont(bold);
        p->setPen(QColor(0xf2, 0xf2, 0xf3));
        p->drawText(QRectF(text.left(), text.top(), text.width(), text.height() / 2), Qt::AlignVCenter | Qt::AlignLeft,
                    p->fontMetrics().elidedText(index.data(TitleRole).toString(), Qt::ElideRight, int(text.width())));
        p->setFont(option.font);
        p->setPen(theme::mutedColor());
        const QString line = index.data(AddressRole).toString() + QStringLiteral("  ·  ") + index.data(StateTextRole).toString();
        p->drawText(QRectF(text.left(), text.center().y(), text.width(), text.height() / 2), Qt::AlignVCenter | Qt::AlignLeft,
                    p->fontMetrics().elidedText(line, Qt::ElideRight, int(text.width())));
        p->restore();
    }
};

QString stateText(Atem::ConnectionState state)
{
    switch (state) {
        case Atem::ConnectionState::Connected: return QStringLiteral("connected");
        case Atem::ConnectionState::Connecting: return QStringLiteral("connecting…");
        case Atem::ConnectionState::Failed: return QStringLiteral("failed");
        default: return QStringLiteral("not connected");
    }
}

} // namespace

SwitcherListWidget::SwitcherListWidget(AtemRemoteOscApp *app, QWidget *parent)
    : QWidget(parent)
    , m_app(app)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout;
    auto *caption = new QLabel(QStringLiteral("SWITCHERS"), this);
    caption->setObjectName(QStringLiteral("caption"));
    QFont f = caption->font();
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    caption->setFont(f);
    header->addWidget(caption);
    header->addStretch();
    m_btnRemove = new QPushButton(QStringLiteral("−"), this);
    m_btnRemove->setToolTip(QStringLiteral("Remove the selected switcher"));
    m_btnAdd = new QPushButton(QStringLiteral("+"), this);
    m_btnAdd->setToolTip(QStringLiteral("Add a switcher"));
    for (auto *b : {m_btnRemove, m_btnAdd}) {
        b->setFixedSize(28, 24);
        header->addWidget(b);
    }
    layout->addLayout(header);

    m_stack = new QStackedWidget(this);
    auto *empty = new QLabel(QStringLiteral("No switcher yet.\nPress + to add one."), m_stack);
    empty->setObjectName(QStringLiteral("hint"));
    empty->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(empty);

    m_list = new QListWidget(m_stack);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setItemDelegate(new CardDelegate(m_list));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setMouseTracking(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setStyleSheet(QStringLiteral("QListWidget { background:transparent; outline:0; }"));
    m_stack->addWidget(m_list);
    layout->addWidget(m_stack, 1);

    connect(m_btnAdd, &QPushButton::clicked, this, &SwitcherListWidget::addSwitcher);
    connect(m_btnRemove, &QPushButton::clicked, this, &SwitcherListWidget::onRemoveClicked);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &SwitcherListWidget::onItemSelectionChanged);

    connect(m_app, &AtemRemoteOscApp::switcherListChanged, this, &SwitcherListWidget::refreshList);
    connect(m_app, &AtemRemoteOscApp::switcherUpdated, this, &SwitcherListWidget::refreshList);

    refreshList();
}

int SwitcherListWidget::selectedIndex() const { return m_list->currentRow(); }

void SwitcherListWidget::refreshList()
{
    const int current = m_list->currentRow();
    // Rebuilding the list is not a selection change: without this, every connection state change would
    // reload the editable fields and overwrite what is being typed
    const QSignalBlocker blocker(m_list);
    m_list->clear();

    const auto &switchers = m_app->switchers();
    for (int i = 0; i < switchers.size(); ++i) {
        auto *sw = switchers[i];
        const auto state = sw->backend ? sw->backend->connectionState() : Atem::ConnectionState::Disconnected;
        QString title = sw->config.nickname.isEmpty() ? QStringLiteral("Switcher %1").arg(i + 1) : sw->config.nickname;
        if (sw->backend && state == Atem::ConnectionState::Connected && !sw->backend->switcherName().isEmpty())
            title = sw->backend->switcherName() + (sw->config.nickname.isEmpty() ? QString() : QStringLiteral(" – ") + sw->config.nickname);
        auto *item = new QListWidgetItem(m_list);
        item->setData(TitleRole, title);
        item->setData(AddressRole, sw->config.ip.isEmpty() ? QStringLiteral("no address") : sw->config.ip);
        item->setData(StateRole, static_cast<int>(state));
        item->setData(StateTextRole, stateText(state));
        item->setToolTip(title);
    }

    m_stack->setCurrentIndex(m_list->count() > 0 ? 1 : 0);
    m_btnRemove->setEnabled(m_list->count() > 0);

    if (current >= 0 && current < m_list->count()) {
        m_list->setCurrentRow(current);
    } else if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }

    // The very first selection (nothing was selected before) still has to reach the editor
    if (current < 0 && m_list->count() > 0) emit selectionChanged(m_list->currentRow());
    if (m_list->count() == 0) emit selectionChanged(-1);
}

void SwitcherListWidget::addSwitcher()
{
    SwitcherConfig cfg;
    cfg.ip.clear(); // to be filled in by the user
    cfg.outgoingIp = QStringLiteral("127.0.0.1");
    cfg.outgoingPort = 53000;
    m_app->addSwitcher(cfg);
    m_list->setCurrentRow(m_list->count() - 1);
}

void SwitcherListWidget::onRemoveClicked()
{
    const int row = m_list->currentRow();
    if (row >= 0) m_app->removeSwitcher(row);
}

void SwitcherListWidget::onItemSelectionChanged() { emit selectionChanged(m_list->currentRow()); }
