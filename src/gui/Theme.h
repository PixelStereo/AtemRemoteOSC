#pragma once

#include <QColor>
#include <QString>

class QApplication;

// Dark interface in the style of NyxMap: Fusion, dark grey palette, one accent colour (very light grey by default)
// used for the selection, the current tab and the buttons that are on.
namespace theme {
QColor defaultAccent();
QColor accent();
void setAccent(const QColor &c); // saved in the settings, then the application is restyled
QColor onAccent();               // text drawn on the accent: dark or light, whichever reads
QString css(int alpha = 255);    // "#rrggbb", or "rgba(r, g, b, a)" when alpha < 255
QString tabStyle();
void applyToApplication(QApplication &app); // palette and global stylesheet (called again on a change)
} // namespace theme
