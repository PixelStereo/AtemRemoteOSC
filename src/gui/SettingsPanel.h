#pragma once

#include <QWidget>

// Look of the interface: the accent colour, shared with the other Pixel Stereo applications.
class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);
};
