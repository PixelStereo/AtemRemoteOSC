#pragma once

#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>
#include "../app/AtemRemoteOscApp.h"

// The sidebar: one card per switcher (state dot, name, address, state), with + and − in its header.
class SwitcherListWidget : public QWidget {
    Q_OBJECT
public:
    explicit SwitcherListWidget(AtemRemoteOscApp *app, QWidget *parent = nullptr);

    int selectedIndex() const;
    void addSwitcher(); // what the + button does

signals:
    void selectionChanged(int index);

public slots:
    void refreshList();

private slots:
    void onRemoveClicked();
    void onItemSelectionChanged();

private:
    AtemRemoteOscApp *m_app = nullptr;
    QListWidget *m_list = nullptr;
    QStackedWidget *m_stack = nullptr; // empty message | list
    QPushButton *m_btnAdd = nullptr;
    QPushButton *m_btnRemove = nullptr;
};
