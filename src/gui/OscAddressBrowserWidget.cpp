#include "OscAddressBrowserWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QGuiApplication>
#include <QClipboard>

OscAddressBrowserWidget::OscAddressBrowserWidget(Osc::AddressRegistry *registry, QWidget *parent)
    : QWidget(parent)
    , m_registry(registry)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Top Filter Bar
    auto *filterLayout = new QHBoxLayout;
    filterLayout->setSpacing(8);

    filterLayout->addWidget(new QLabel(QStringLiteral("Filter:"), this));
    m_txtFilter = new QLineEdit(this);
    m_txtFilter->setPlaceholderText(QStringLiteral("Search address, description, or parameter..."));
    m_txtFilter->setClearButtonEnabled(true);
    filterLayout->addWidget(m_txtFilter);

    filterLayout->addWidget(new QLabel(QStringLiteral("Category:"), this));
    m_cmbCategory = new QComboBox(this);
    m_cmbCategory->addItem(QStringLiteral("All Categories"));
    m_cmbCategory->addItems({
        QStringLiteral("M/E"),
        QStringLiteral("Transitions"),
        QStringLiteral("Upstream Keyers"),
        QStringLiteral("Downstream Keyers"),
        QStringLiteral("SuperSource"),
        QStringLiteral("Audio"),
        QStringLiteral("Audio Fairlight"),
        QStringLiteral("MultiView"),
        QStringLiteral("Display Clock"),
        QStringLiteral("Recording"),
        QStringLiteral("Streaming"),
        QStringLiteral("Camera PTZ"),
        QStringLiteral("Macros"),
        QStringLiteral("HyperDeck"),
        QStringLiteral("Media Player"),
        QStringLiteral("Aux")
    });
    filterLayout->addWidget(m_cmbCategory);

    m_btnCopy = new QPushButton(QStringLiteral("Copy Address"), this);
    filterLayout->addWidget(m_btnCopy);

    layout->addLayout(filterLayout);

    // Table View with Proxy Model
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_registry);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1); // Filter all columns

    m_table = new QTableView(this);
    m_table->setModel(m_proxyModel);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(Osc::AddressRegistry::ColPattern, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(Osc::AddressRegistry::ColDescription, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(Osc::AddressRegistry::ColType, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(Osc::AddressRegistry::ColCategory, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(Osc::AddressRegistry::ColFeedback, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(Osc::AddressRegistry::ColLiveValue, QHeaderView::ResizeToContents);

    layout->addWidget(m_table);

    connect(m_txtFilter, &QLineEdit::textChanged, this, &OscAddressBrowserWidget::onFilterChanged);
    connect(m_cmbCategory, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OscAddressBrowserWidget::onCategoryChanged);
    connect(m_btnCopy, &QPushButton::clicked, this, &OscAddressBrowserWidget::onCopyClicked);
}

void OscAddressBrowserWidget::onFilterChanged(const QString &text) {
    m_proxyModel->setFilterFixedString(text);
}

void OscAddressBrowserWidget::onCategoryChanged(int index) {
    if (index == 0) {
        m_proxyModel->setFilterFixedString(m_txtFilter->text());
    } else {
        m_proxyModel->setFilterFixedString(m_cmbCategory->currentText());
    }
}

void OscAddressBrowserWidget::onCopyClicked() {
    QModelIndex current = m_table->currentIndex();
    if (current.isValid()) {
        QModelIndex patternIdx = m_proxyModel->index(current.row(), Osc::AddressRegistry::ColPattern);
        QString address = m_proxyModel->data(patternIdx).toString();
        QGuiApplication::clipboard()->setText(address);
    }
}
