#pragma once

#include <QStatusBar>

class QLabel;
class QToolButton;

// One line at the bottom: what is running (OSC input, OSCQuery, switchers), the last message, and a counter of
// errors and warnings that opens the console.
class StatusLine : public QStatusBar {
    Q_OBJECT
public:
    explicit StatusLine(QWidget *parent = nullptr);

    void setSummary(const QString &richText);
    void setMessage(const QString &text, int level); // LogConsoleWidget::Level
    void setAlerts(int errors, int warnings);

signals:
    void alertsClicked();

private:
    QLabel *m_summary = nullptr;
    QLabel *m_message = nullptr;
    QToolButton *m_alerts = nullptr;
};
