#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include "../app/AtemRemoteOscApp.h"

// The page of one switcher: its address and state, and where its OSC feedback goes.
class ConnectionSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionSettingsWidget(AtemRemoteOscApp *app, QWidget *parent = nullptr);

    void setSwitcherIndex(int index);

signals:
    void addRequested(); // from the empty page

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void applySettings();

private:
    void updateUiFromConfig();
    void updateStatus();
    void setPill(const QString &text, const char *state);

    AtemRemoteOscApp *m_app = nullptr;
    int m_currentIndex = -1;

    QStackedWidget *m_stack = nullptr; // empty | switcher
    QLabel *m_lblSwitcherName = nullptr;
    QLabel *m_lblPill = nullptr;
    QLineEdit *m_txtIp = nullptr;
    QLineEdit *m_txtNickname = nullptr;
    QPushButton *m_btnConnect = nullptr;
    QPushButton *m_btnDisconnect = nullptr;

    QLineEdit *m_txtOutgoingIp = nullptr;
    QSpinBox *m_spnOutgoingPort = nullptr;
    QCheckBox *m_chkInvertTBar = nullptr;
    QCheckBox *m_chkAutoReconnect = nullptr;

    bool m_updating = false;
};
