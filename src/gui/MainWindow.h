#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QTabWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include "../app/AtemRemoteOscApp.h"
#include "SwitcherListWidget.h"
#include "ConnectionSettingsWidget.h"
#include "OscAddressBrowserWidget.h"
#include "LogConsoleWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AtemRemoteOscApp *app, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSwitcherSelected(int index);
    void onApplyPortClicked();
    void onOscServerStateChanged(bool listening, quint16 port);
    void onOscQueryChanged();

private:
    void setupUi();
    void updateServerStatusUi(bool listening, quint16 port);
    void updateOscQueryStatus();

    AtemRemoteOscApp *m_app = nullptr;

    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_topSplitter = nullptr;

    SwitcherListWidget *m_switcherList = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    ConnectionSettingsWidget *m_connectionWidget = nullptr;
    OscAddressBrowserWidget *m_addressBrowser = nullptr;
    LogConsoleWidget *m_logConsole = nullptr;

    QSpinBox *m_spnIncomingPort = nullptr;
    QPushButton *m_btnApplyPort = nullptr;
    QLabel *m_lblServerStatus = nullptr;
    QCheckBox *m_chkOscQuery = nullptr;
    QSpinBox *m_spnOscQueryPort = nullptr;
    QLabel *m_lblOscQueryStatus = nullptr;
};
