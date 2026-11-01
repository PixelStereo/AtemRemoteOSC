#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include "../app/AtemOscApp.h"

class SwitcherListWidget : public QWidget {
    Q_OBJECT
public:
    explicit SwitcherListWidget(AtemOscApp *app, QWidget *parent = nullptr);

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
    AtemOscApp *m_app = nullptr;
    QListWidget *m_list = nullptr;
    QPushButton *m_btnAdd = nullptr;
    QPushButton *m_btnRemove = nullptr;
};
