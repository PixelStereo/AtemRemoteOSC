#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include "../app/AtemRemoteOscApp.h"

class ConnectionSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionSettingsWidget(AtemRemoteOscApp *app, QWidget *parent = nullptr);

    void setSwitcherIndex(int index);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void applySettings();

private:
    void updateUiFromConfig();

    AtemRemoteOscApp *m_app = nullptr;
    int m_currentIndex = -1;

    QLabel *m_lblSwitcherName = nullptr;
    QLabel *m_lblStatus = nullptr;
    QLineEdit *m_txtIp = nullptr;
    QLineEdit *m_txtNickname = nullptr;
    QComboBox *m_cmbBackend = nullptr;
    QPushButton *m_btnConnect = nullptr;
    QPushButton *m_btnDisconnect = nullptr;

    QLineEdit *m_txtOutgoingIp = nullptr;
    QSpinBox *m_spnOutgoingPort = nullptr;
    QCheckBox *m_chkInvertTBar = nullptr;
    QCheckBox *m_chkAutoReconnect = nullptr;

    bool m_updating = false;
};
