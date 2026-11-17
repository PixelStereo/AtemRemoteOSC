#include "SettingsPanel.h"
#include "Theme.h"
#include <QColorDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SettingsPanel::SettingsPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    auto *grp = new QGroupBox(QStringLiteral("Accent colour"), this);
    grp->setObjectName(QStringLiteral("section"));
    auto *box = new QVBoxLayout(grp);
    auto *hint = new QLabel(QStringLiteral("Used for the selection, the current tab and the switches that are on."), grp);
    hint->setObjectName(QStringLiteral("hint"));
    box->addWidget(hint);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    for (const char *hex : {"#d6d6db", "#8fb8e8", "#7fd6a6", "#f2b84b", "#f08a7e", "#c49af0"}) {
        auto *swatch = new QPushButton(grp);
        swatch->setFixedSize(30, 24);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setToolTip(QString::fromLatin1(hex));
        swatch->setStyleSheet(QStringLiteral("QPushButton { background:%1; border:1px solid #62626e; border-radius:4px; }"
                                             "QPushButton:hover { border-color:#ffffff; }").arg(QString::fromLatin1(hex)));
        connect(swatch, &QPushButton::clicked, this, [hex] { theme::setAccent(QColor(QString::fromLatin1(hex))); });
        row->addWidget(swatch);
    }
    auto *custom = new QPushButton(QStringLiteral("Other…"), grp);
    connect(custom, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(theme::accent(), this, QStringLiteral("Accent colour"));
        if (c.isValid()) theme::setAccent(c);
    });
    auto *reset = new QPushButton(QStringLiteral("Default"), grp);
    connect(reset, &QPushButton::clicked, this, [] { theme::setAccent(theme::defaultAccent()); });
    row->addSpacing(6);
    row->addWidget(custom);
    row->addWidget(reset);
    row->addStretch();
    box->addLayout(row);
    layout->addWidget(grp);
    layout->addStretch();
}
