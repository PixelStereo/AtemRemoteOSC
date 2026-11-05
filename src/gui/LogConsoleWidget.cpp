#include "LogConsoleWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>

LogConsoleWidget::LogConsoleWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_console = new QPlainTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(1000);
    QFont monoFont(QStringLiteral("Menlo, Consolas, Monaco, monospace"));
    monoFont.setPointSize(11);
    m_console->setFont(monoFont);
    layout->addWidget(m_console);

    auto *bottomBar = new QHBoxLayout;
    bottomBar->setContentsMargins(0, 0, 0, 0);

    m_chkLogDebug = new QCheckBox(QStringLiteral("Log incoming OSC messages"), this);
    bottomBar->addWidget(m_chkLogDebug);

    bottomBar->addStretch();

    m_btnClear = new QPushButton(QStringLiteral("Clear Log"), this);
    bottomBar->addWidget(m_btnClear);

    layout->addLayout(bottomBar);

    connect(m_chkLogDebug, &QCheckBox::toggled, this, &LogConsoleWidget::logDebugOscChanged);
    connect(m_btnClear, &QPushButton::clicked, this, &LogConsoleWidget::clearLog);
}

bool LogConsoleWidget::logDebugOsc() const {
    return m_chkLogDebug->isChecked();
}

void LogConsoleWidget::setLogDebugOsc(bool enable) {
    m_chkLogDebug->setChecked(enable);
}

void LogConsoleWidget::appendLog(const QString &text) {
    appendLogAt(QDateTime::currentDateTime(), text);
}

void LogConsoleWidget::appendLogAt(const QDateTime &time, const QString &text) {
    m_console->appendPlainText(QStringLiteral("[%1] %2").arg(time.toString(QStringLiteral("hh:mm:ss.zzz")), text));
}

void LogConsoleWidget::clearLog() {
    m_console->clear();
}
