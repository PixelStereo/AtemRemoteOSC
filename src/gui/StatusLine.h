#pragma once

#include <QFrame>

class QLabel;
class QToolButton;

// The bar across the top of the window: the name, what is running (OSC input, OSCQuery, switchers), the last
// message, and a counter of errors and warnings that opens the log.
class StatusLine : public QFrame {
    Q_OBJECT
public:
    explicit StatusLine(QWidget *parent = nullptr);

    void setSummary(const QString &richText);
    void setMessage(const QString &text, int level); // LogConsoleWidget::Level
    void setAlerts(int errors, int warnings);

signals:
    void alertsClicked();
    void messageClicked();

private:
    QLabel *m_summary = nullptr;
    QToolButton *m_message = nullptr;
    QToolButton *m_alerts = nullptr;
};
