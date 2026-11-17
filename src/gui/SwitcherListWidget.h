#pragma once

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>
#include "../app/AtemRemoteOscApp.h"

// A strip of chips across the top of the Switchers page: one per switcher (state dot, name, address), then + and ×.
class SwitcherListWidget : public QWidget {
    Q_OBJECT
public:
    explicit SwitcherListWidget(AtemRemoteOscApp *app, QWidget *parent = nullptr);

    int selectedIndex() const { return m_selected; }
    void addSwitcher(); // what the + chip does

signals:
    void selectionChanged(int index);

public slots:
    void refreshList();

private:
    void select(int index);

    AtemRemoteOscApp *m_app = nullptr;
    QHBoxLayout *m_chips = nullptr;
    QButtonGroup m_group;
    QPushButton *m_btnAdd = nullptr;
    QPushButton *m_btnRemove = nullptr;
    int m_selected = -1;
    QVector<QPushButton *> m_buttons;
};
