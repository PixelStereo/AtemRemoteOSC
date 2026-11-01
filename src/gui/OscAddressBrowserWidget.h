#pragma once

#include <QWidget>
#include <QTableView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include "../osc/OscAddressRegistry.h"

class OscAddressBrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit OscAddressBrowserWidget(Osc::AddressRegistry *registry, QWidget *parent = nullptr);

private slots:
    void onFilterChanged(const QString &text);
    void onCategoryChanged(int index);
    void onCopyClicked();

private:
    Osc::AddressRegistry *m_registry = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;

    QLineEdit *m_txtFilter = nullptr;
    QComboBox *m_cmbCategory = nullptr;
    QTableView *m_table = nullptr;
    QPushButton *m_btnCopy = nullptr;
};
