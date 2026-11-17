#include "StatusLine.h"
#include "Theme.h"
#include <QLabel>
#include <QToolButton>

StatusLine::StatusLine(QWidget *parent) : QStatusBar(parent)
{
    setSizeGripEnabled(false);
    m_summary = new QLabel(this);
    m_summary->setTextFormat(Qt::RichText);
    QStatusBar::addWidget(m_summary);

    m_message = new QLabel(this);
    m_message->setTextFormat(Qt::PlainText);
    m_message->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // cut short, never wider than its room
    QStatusBar::addWidget(m_message, 1);

    m_alerts = new QToolButton(this);
    m_alerts->setCursor(Qt::PointingHandCursor);
    m_alerts->setAutoRaise(true);
    m_alerts->hide();
    QStatusBar::addPermanentWidget(m_alerts);
    connect(m_alerts, &QToolButton::clicked, this, &StatusLine::alertsClicked);
}

void StatusLine::setSummary(const QString &richText) { m_summary->setText(richText); }

void StatusLine::setMessage(const QString &text, int level)
{
    m_message->setText(QStringLiteral("   ") + text.simplified());
    m_message->setToolTip(text);
    const char *colour = level == 2 ? "color:#ff8f82;" : level == 1 ? "color:#f2b84b;" : "";
    m_message->setStyleSheet(QString::fromLatin1(colour));
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
    m_alerts->setToolTip(QStringLiteral("Open the console"));
    const bool bad = errors > 0;
    m_alerts->setStyleSheet(QStringLiteral("QToolButton { color:%1; background:%2; border:1px solid %1; border-radius:9px; padding:1px 9px; }"
                                           "QToolButton:hover { background:%3; }")
                                .arg(bad ? "#ff8f82" : "#f2b84b", bad ? "#3a1512" : "#3a2a12", bad ? "#4a1c18" : "#4a3616"));
    m_alerts->show();
}
