#include "LogConsoleWidget.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QTextCharFormat>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

LogConsoleWidget::LogConsoleWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    m_console = new QPlainTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(1000);
    QFont monoFont(QStringLiteral("Menlo, Consolas, Monaco, monospace"));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(11);
    m_console->setFont(monoFont);
    layout->addWidget(m_console, 1);

    auto *bottomBar = new QHBoxLayout;
    m_chkLogDebug = new QCheckBox(QStringLiteral("Log every incoming OSC message"), this);
    bottomBar->addWidget(m_chkLogDebug);
    bottomBar->addStretch();
    m_btnClear = new QPushButton(QStringLiteral("Clear"), this);
    bottomBar->addWidget(m_btnClear);
    layout->addLayout(bottomBar);

    connect(m_chkLogDebug, &QCheckBox::toggled, this, &LogConsoleWidget::logDebugOscChanged);
    connect(m_btnClear, &QPushButton::clicked, this, &LogConsoleWidget::clearLog);
}

bool LogConsoleWidget::logDebugOsc() const { return m_chkLogDebug->isChecked(); }
void LogConsoleWidget::setLogDebugOsc(bool enable) { m_chkLogDebug->setChecked(enable); }

LogConsoleWidget::Level LogConsoleWidget::levelOf(const QString &text)
{
    const QString t = text.toLower();
    for (const char *word : {"fail", "error", "could not", "unable", "cannot", "not found", "refused", "lost"})
        if (t.contains(QLatin1String(word))) return Error;
    for (const char *word : {"warning", "retry", "attempt", "limit", "ignored", "enter the ip"})
        if (t.contains(QLatin1String(word))) return Warning;
    return Info;
}

void LogConsoleWidget::appendLog(const QString &text) { appendLogAt(QDateTime::currentDateTime(), text); }

void LogConsoleWidget::appendLogAt(const QDateTime &time, const QString &text)
{
    const Level level = levelOf(text);
    QTextCharFormat format;
    if (level == Error) format.setForeground(theme::errorColor());
    else if (level == Warning) format.setForeground(theme::warnColor());
    QTextCursor cursor(m_console->document());
    cursor.movePosition(QTextCursor::End);
    if (!m_console->document()->isEmpty()) cursor.insertBlock();
    cursor.insertText(QStringLiteral("%1  %2").arg(time.toString(QStringLiteral("hh:mm:ss.zzz")), text), format);
    m_console->verticalScrollBar()->setValue(m_console->verticalScrollBar()->maximum());
    ++m_counts[level];
    if (level != Info) emit countsChanged();
    emit messageLogged(text, level);
}

void LogConsoleWidget::clearLog()
{
    m_console->clear();
    m_counts[Info] = m_counts[Warning] = m_counts[Error] = 0;
    emit countsChanged();
}
