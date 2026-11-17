#include "StatusLine.h"
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

StatusLine::StatusLine(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("header"));
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(16, 8, 12, 8);
    row->setSpacing(18);

    auto *name = new QLabel(QStringLiteral("ATEM REMOTE OSC"), this);
    QFont f = name->font();
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.6);
    name->setFont(f);
    row->addWidget(name);

    m_summary = new QLabel(this);
    m_summary->setTextFormat(Qt::RichText);
    row->addWidget(m_summary);

    m_message = new QToolButton(this);
    m_message->setObjectName(QStringLiteral("ticker"));
    m_message->setCursor(Qt::PointingHandCursor);
    m_message->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_message->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // cut short, never wider than its room
    row->addWidget(m_message, 1);
    connect(m_message, &QToolButton::clicked, this, &StatusLine::messageClicked);

    m_alerts = new QToolButton(this);
    m_alerts->setCursor(Qt::PointingHandCursor);
    m_alerts->setAutoRaise(true);
    m_alerts->hide();
    row->addWidget(m_alerts);
    connect(m_alerts, &QToolButton::clicked, this, &StatusLine::alertsClicked);
}

void StatusLine::setSummary(const QString &richText) { m_summary->setText(richText); }

void StatusLine::setMessage(const QString &text, int level)
{
    const QString one = text.simplified();
    m_message->setToolTip(one);
    m_message->setText(m_message->fontMetrics().elidedText(one, Qt::ElideRight, qMax(60, m_message->width() - 16)));
    const char *colour = level == 2 ? "color:#ff8f82;" : level == 1 ? "color:#f2b84b;" : "color:#9a9aa3;";
    m_message->setStyleSheet(QStringLiteral("QToolButton#ticker { %1 border:0; background:transparent; text-align:right; }")
                                 .arg(QString::fromLatin1(colour)));
}

void StatusLine::setAlerts(int errors, int warnings)
{
    if (!errors && !warnings) {
        m_alerts->hide();
        return;
    }
    QStringList parts;
    if (errors) parts << QStringLiteral("%1 error%2").arg(errors).arg(errors > 1 ? "s" : "");
    if (warnings) parts << QStringLiteral("%1 warning%2").arg(warnings).arg(warnings > 1 ? "s" : "");
    m_alerts->setText(QStringLiteral("● ") + parts.join(QStringLiteral(" · ")));
    m_alerts->setToolTip(QStringLiteral("Open the log"));
    const bool bad = errors > 0;
    m_alerts->setStyleSheet(QStringLiteral("QToolButton { color:%1; background:%2; border:1px solid %1; border-radius:11px; padding:2px 11px; }"
                                           "QToolButton:hover { background:%3; }")
                                .arg(bad ? "#ff8f82" : "#f2b84b", bad ? "#3a1512" : "#3a2a12", bad ? "#4a1c18" : "#4a3616"));
    m_alerts->show();
}
