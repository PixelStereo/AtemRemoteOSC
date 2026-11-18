#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QPainter;
class QRectF;

// The graphite interface shared with NyxMap and MpOsc: Fusion, dark neutral greys, one accent colour (very light grey
// by default) for the selection, the current tab and what is on, square check boxes drawn by the theme.
namespace theme {
QColor defaultAccent();
QColor accent();
void setAccent(const QColor &c); // saved in the settings, then the application is restyled
QColor onAccent();               // text drawn on the accent: dark or light, whichever reads
QString css(int alpha = 255);    // "#rrggbb", or "rgba(r, g, b, a)" when alpha < 255
void applyToApplication(QApplication &app); // palette and global stylesheet (called again on a change)

// Colours of a state, the same everywhere (dots, pills, the status line)
QColor okColor();
QColor warnColor();
QColor errorColor();
QColor mutedColor();

} // namespace theme
