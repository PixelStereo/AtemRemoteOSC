#pragma once

#include <QDockWidget>
#include <QMainWindow>
#include <QSplitter>
#include <QTabWidget>
#include "../app/AtemRemoteOscApp.h"
#include "ConnectionSettingsWidget.h"
#include "LogConsoleWidget.h"
#include "NetworkPanel.h"
#include "OscAddressBrowserWidget.h"
#include "SettingsPanel.h"
#include "StatusLine.h"
#include "SwitcherListWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AtemRemoteOscApp *app, QWidget *parent = nullptr);
    ~MainWindow() override;

    void showConsole(bool show);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void setupMenus();
    void updateSummary();

    AtemRemoteOscApp *m_app = nullptr;

    QSplitter *m_splitter = nullptr;
    SwitcherListWidget *m_switcherList = nullptr;
    QTabWidget *m_tabs = nullptr;
    ConnectionSettingsWidget *m_connectionWidget = nullptr;
    NetworkPanel *m_network = nullptr;
    OscAddressBrowserWidget *m_addressBrowser = nullptr;
    SettingsPanel *m_settings = nullptr;
    QDockWidget *m_consoleDock = nullptr;
    LogConsoleWidget *m_logConsole = nullptr;
    StatusLine *m_status = nullptr;
};
