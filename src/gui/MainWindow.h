#pragma once

#include <QButtonGroup>
#include <QMainWindow>
#include <QStackedWidget>
#include "../app/AtemRemoteOscApp.h"
#include "ConnectionSettingsWidget.h"
#include "LogConsoleWidget.h"
#include "NetworkPanel.h"
#include "OscAddressBrowserWidget.h"
#include "SettingsPanel.h"
#include "StatusLine.h"
#include "SwitcherListWidget.h"

// A header bar across the top (what runs, the last message), a navigation rail on the left, and one page at a time:
// Switchers, Network, Addresses, Log, Settings.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AtemRemoteOscApp *app, QWidget *parent = nullptr);
    ~MainWindow() override;

    enum Page { PageSwitchers, PageNetwork, PageAddresses, PageLog, PageSettings };
    void showPage(int page);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void setupMenus();
    void updateSummary();

    AtemRemoteOscApp *m_app = nullptr;

    QStackedWidget *m_pages = nullptr;
    QButtonGroup m_nav;
    SwitcherListWidget *m_switcherList = nullptr;
    ConnectionSettingsWidget *m_connectionWidget = nullptr;
    NetworkPanel *m_network = nullptr;
    OscAddressBrowserWidget *m_addressBrowser = nullptr;
    LogConsoleWidget *m_logConsole = nullptr;
    SettingsPanel *m_settings = nullptr;
    StatusLine *m_status = nullptr;
};
