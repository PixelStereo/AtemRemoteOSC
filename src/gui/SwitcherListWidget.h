#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include "../app/AtemRemoteOscApp.h"

class SwitcherListWidget : public QWidget {
    Q_OBJECT
public:
    explicit SwitcherListWidget(AtemRemoteOscApp *app, QWidget *parent = nullptr);

    int selectedIndex() const;

signals:
    void selectionChanged(int index);

public slots:
    void refreshList();

private slots:
    void onAddClicked();
    void onRemoveClicked();
    void onItemSelectionChanged();

private:
    AtemRemoteOscApp *m_app = nullptr;
    QListWidget *m_list = nullptr;
    QPushButton *m_btnAdd = nullptr;
    QPushButton *m_btnRemove = nullptr;
};
