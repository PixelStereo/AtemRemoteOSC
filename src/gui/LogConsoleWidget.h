#pragma once

#include <QDateTime>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QWidget>

// Everything the application says, in time order; errors and warnings are coloured and counted.
class LogConsoleWidget : public QWidget {
    Q_OBJECT
public:
    enum Level { Info, Warning, Error };

    explicit LogConsoleWidget(QWidget *parent = nullptr);

    bool logDebugOsc() const;
    void setLogDebugOsc(bool enable);
    int count(Level level) const { return m_counts[level]; }
    static Level levelOf(const QString &text);

public slots:
    void appendLog(const QString &text);
    void appendLogAt(const QDateTime &time, const QString &text);
    void clearLog();

signals:
    void logDebugOscChanged(bool enable);
    void messageLogged(const QString &text, int level); // a Level
    void countsChanged();

private:
    QPlainTextEdit *m_console = nullptr;
    QCheckBox *m_chkLogDebug = nullptr;
    QPushButton *m_btnClear = nullptr;
    int m_counts[3] = {0, 0, 0};
};
