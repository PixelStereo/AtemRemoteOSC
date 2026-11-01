#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QPushButton>

class LogConsoleWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogConsoleWidget(QWidget *parent = nullptr);

    bool logDebugOsc() const;
    void setLogDebugOsc(bool enable);

public slots:
    void appendLog(const QString &text);
    void clearLog();

signals:
    void logDebugOscChanged(bool enable);

private:
    QPlainTextEdit *m_console = nullptr;
    QCheckBox *m_chkLogDebug = nullptr;
    QPushButton *m_btnClear = nullptr;
};
