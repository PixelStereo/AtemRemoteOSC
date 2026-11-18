#include "Theme.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPalette>
#include <QProxyStyle>
#include <QSettings>
#include <QStyleFactory>
#include <algorithm>
#include <pxs/Theme.h>

namespace theme {

namespace {
QColor g_accent;
bool g_loaded = false;

// A selected row of a list is tinted, not filled: its content stays readable
class SoftSelection : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() == QEvent::Polish) {
            if (auto *view = qobject_cast<QAbstractItemView *>(o)) {
                QPalette p = view->palette();
                const QColor base = p.color(QPalette::Base), a = accent();
                const double k = 0.30;
                p.setColor(QPalette::Highlight, QColor(int(base.red() + (a.red() - base.red()) * k),
                                                       int(base.green() + (a.green() - base.green()) * k),
                                                       int(base.blue() + (a.blue() - base.blue()) * k)));
                p.setColor(QPalette::HighlightedText, QColor(240, 240, 242));
                view->setPalette(p);
            }
        }
        return false;
    }
};

} // namespace

QColor defaultAccent() { return QColor(214, 214, 219); }

QColor accent()
{
    if (!g_loaded) {
        const QColor c(QSettings().value(QStringLiteral("ui/accent")).toString());
        g_accent = c.isValid() ? c : defaultAccent();
        g_loaded = true;
    }
    return g_accent;
}

void setAccent(const QColor &c)
{
    g_accent = c.isValid() ? c : defaultAccent();
    g_loaded = true;
    QSettings().setValue(QStringLiteral("ui/accent"), g_accent.name());
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) applyToApplication(*app);
}

QColor onAccent()
{
    const QColor a = accent();
    const double l = 0.2126 * a.redF() + 0.7152 * a.greenF() + 0.0722 * a.blueF();
    return l > 0.55 ? QColor(26, 26, 29) : QColor(240, 240, 243);
}

QString css(int alpha)
{
    const QColor a = accent();
    if (alpha >= 255) return a.name();
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(a.red()).arg(a.green()).arg(a.blue()).arg(alpha);
}

QColor okColor() { return QColor(0x5f, 0xc9, 0x7a); }
QColor warnColor() { return QColor(0xf2, 0xb8, 0x4b); }
QColor errorColor() { return QColor(0xff, 0x8f, 0x82); }
QColor mutedColor() { return QColor(0x9a, 0x9a, 0xa3); }

void applyToApplication(QApplication &app)
{
    static bool filterInstalled = false;
    if (!filterInstalled) {
        app.installEventFilter(new SoftSelection(&app));
        filterInstalled = true;
    }
    // The graphite look shared by every Pixel Stereo application; the accent stays the one the user chose
    pxs::Palette palette = pxs::graphite();
    palette.link = accent();
    pxs::apply(app, palette, QStringLiteral(
        // Cards: the grouped settings
        "QGroupBox#section { border:1px solid @border; border-radius:6px; margin-top:12px; padding:14px 10px 10px 10px; }"
        "QGroupBox#section::title { subcontrol-origin:margin; left:11px; padding:0 5px; color:@text; font-weight:600; }"
        "QLabel#caption { color:@muted; }"
        "QLabel#title { font-size:15px; font-weight:600; color:@text; }"
        "QLabel#hint { color:@muted; }"
        // A state, in words
        "QLabel#pill { border:1px solid @border; border-radius:9px; padding:2px 10px; color:@muted; }"
        "QLabel#pill[state=\"ok\"] { color:#5fc97a; border-color:#3b7a4c; background:@window; }"
        "QLabel#pill[state=\"busy\"] { color:#f2b84b; border-color:#8a6a2a; background:@window; }"
        "QLabel#pill[state=\"bad\"] { color:#ff8f82; border-color:#8a3a32; background:@window; }"
        // Header bar, rail, chips, hero: the window's own structure
        "QFrame#header { background:@base; border-bottom:1px solid @border; }"
        "QFrame#rail { background:@base; border-right:1px solid @border; }"
        "QPushButton#nav { background:transparent; border:0; border-left:3px solid transparent; border-radius:0;"
        " text-align:left; padding:10px 18px; color:@muted; font-size:14px; }"
        "QPushButton#nav:hover { background:@button; color:@text; }"
        "QPushButton#nav:checked { background:@button; color:@text; font-weight:600; border-left:3px solid @link; }"
        "QPushButton#chip { background:@button; border:1px solid @border; border-radius:15px; padding:6px 16px; color:@text; }"
        "QPushButton#chip:hover { border-color:@focus; color:@text; }"
        "QPushButton#chip:checked { background:@midlight; border:1px solid @link; color:@text; font-weight:600; }"
        "QPushButton#chip:disabled { color:@border; border-color:@midlight; }"
        "QFrame#hero { background:@button; border:1px solid @border; border-radius:10px; }"
        "QLabel#display { font-size:24px; font-weight:600; color:@text; }"
        "QPushButton#bigButton { min-width:150px; min-height:44px; font-size:15px; border-radius:22px; }"
        "QPushButton[role=\"stop\"] { background:@button; color:@text; font-weight:bold; border:1px solid #9a4a42;"
        " border-radius:3px; padding:5px 14px; min-width:84px; }"
        "QPushButton[role=\"stop\"]:hover { background:#60302b; }"
        "QPushButton#bigButton[role=\"go\"], QPushButton#bigButton[role=\"stop\"] { border-radius:22px; }"
        "QSplitter::handle { background:@fill; margin:1px; }"
        "QSplitter::handle:horizontal { width:5px; }"
        "QSplitter::handle:vertical { height:5px; }"
        "QSplitter::handle:hover { background:@link; }"
        "QHeaderView::section { background:@midlight; color:@text; border:0; border-right:1px solid @border;"
        " border-bottom:1px solid @border; padding:4px 6px; }"
        "QMenu::separator { height:1px; background:@fill; margin:5px 8px; }"
        "QStatusBar { color:@muted; }"
        "QStatusBar::item { border:0; }"
        "QDockWidget { font-weight:600; }"
        "QDockWidget::title { background:@window; padding:5px 8px; border-bottom:1px solid @border; }"
        "QLineEdit, QSpinBox, QComboBox { border-radius:4px; padding:3px 6px; }"
        // A button that is on shows it: the accent, not Fusion's barely darker shade
        "QPushButton:checked, QToolButton:checked { background:@link; color:@window; border:1px solid @link; border-radius:3px; }"
        // The main action, like NyxMap's GO
        "QPushButton[role=\"go\"] { background:@button; color:@text; font-weight:bold; border:1px solid #3f9a52;"
        " border-radius:3px; padding:5px 14px; min-width:84px; }"
        "QPushButton[role=\"go\"]:hover { background:#27603a; }"
        "QPushButton[role=\"go\"]:disabled { background:@midlight; color:@border; border-color:@border; }"
        "QPlainTextEdit { background:@base; color:@link; border:1px solid @border; }")
        );
}

} // namespace theme
