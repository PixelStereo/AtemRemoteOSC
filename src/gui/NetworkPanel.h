#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>
#include "../app/AtemRemoteOscApp.h"

// What AtemRemoteOsc listens on: the OSC input port, OSCQuery, and the addresses a controller should send to.
class NetworkPanel : public QWidget {
    Q_OBJECT
public:
    explicit NetworkPanel(AtemRemoteOscApp *app, QWidget *parent = nullptr);

private:
    void updateOscStatus();
    void updateQueryStatus();
    void applyPort();
    void applyOscQuery();
    void setPill(QLabel *label, const QString &text, const char *state);

    AtemRemoteOscApp *m_app = nullptr;
    QSpinBox *m_spnPort = nullptr;
    QPushButton *m_btnApply = nullptr;
    QLabel *m_lblOsc = nullptr;
    QCheckBox *m_chkQuery = nullptr;
    QSpinBox *m_spnQueryPort = nullptr;
    QLabel *m_lblQuery = nullptr;
};
