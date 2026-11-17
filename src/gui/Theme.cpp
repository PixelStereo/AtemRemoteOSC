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

// Fusion, with the check boxes drawn by paintCheck
class CheckStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;
    void drawPrimitive(PrimitiveElement pe, const QStyleOption *o, QPainter *p, const QWidget *w = nullptr) const override
    {
        if (pe == PE_IndicatorCheckBox || pe == PE_IndicatorItemViewItemCheck) {
            const QRectF r(o->rect);
            const double side = std::min(r.width(), r.height());
            const QRectF box(r.center().x() - side / 2, r.center().y() - side / 2, side, side);
            paintCheck(*p, box, o->state & State_On, o->state & State_MouseOver, o->state & State_Enabled);
            return;
        }
        QProxyStyle::drawPrimitive(pe, o, p, w);
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

void paintCheck(QPainter &p, const QRectF &box, bool on, bool hover, bool enabled)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QColor a = accent();
    if (!enabled) a.setAlphaF(0.45f);
    const QColor edge = !enabled ? QColor(80, 80, 86) : hover ? QColor(170, 170, 178) : QColor(120, 120, 128);
    p.setPen(QPen(on ? a : edge, 1.2));
    p.setBrush(QColor(24, 24, 27));
    p.drawRoundedRect(box.adjusted(0.6, 0.6, -0.6, -0.6), 3, 3);
    if (on) {
        p.setPen(Qt::NoPen);
        p.setBrush(a);
        p.drawRoundedRect(box.adjusted(3.2, 3.2, -3.2, -3.2), 1.5, 1.5);
    }
    p.restore();
}

QString tabStyle()
{
    return QStringLiteral("QTabBar::tab { background:#25252b; color:#c3c3cd; padding:6px 14px; margin-right:3px;"
                          " font-weight:600; border:1px solid #62626e; border-bottom:3px solid #44444e;"
                          " border-top-left-radius:4px; border-top-right-radius:4px; }"
                          "QTabBar::tab:selected { background:#4c4c59; color:#ffffff; border-color:#9494a3;"
                          " border-bottom:3px solid %1; }"
                          "QTabBar::tab:hover:!selected { background:#383841; color:#ffffff; border-color:#9292a0; }"
                          "QTabBar::tab:disabled { color:#777782; border-color:#44444e; }"
                          "QTabWidget::pane { border:1px solid #62626e; }")
        .arg(css());
}

void applyToApplication(QApplication &app)
{
    static bool filterInstalled = false;
    if (!filterInstalled) {
        app.installEventFilter(new SoftSelection(&app));
        filterInstalled = true;
    }
    app.setStyle(new CheckStyle(QStyleFactory::create(QStringLiteral("Fusion"))));
    QPalette p;
    const QColor base(30, 30, 33), window(40, 40, 44), text(225, 225, 228), a = accent();
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(52, 52, 57));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::Highlight, a);
    p.setColor(QPalette::HighlightedText, onAccent());
    p.setColor(QPalette::Link, a);
    p.setColor(QPalette::PlaceholderText, QColor(130, 130, 135));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 125));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 125));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 125));
    app.setPalette(p);
    app.setStyleSheet(tabStyle() + QStringLiteral(
        // Cards: the grouped settings
        "QGroupBox#section { border:1px solid #62626e; border-radius:6px; margin-top:12px; padding:14px 10px 10px 10px; }"
        "QGroupBox#section::title { subcontrol-origin:margin; left:11px; padding:0 5px; color:#f2f2f3; font-weight:600; }"
        "QLabel#caption { color:#9a9aa3; }"
        "QLabel#title { font-size:15px; font-weight:600; color:#f2f2f3; }"
        "QLabel#hint { color:#9a9aa3; }"
        // A state, in words
        "QLabel#pill { border:1px solid #62626e; border-radius:9px; padding:2px 10px; color:#c3c3cd; }"
        "QLabel#pill[state=\"ok\"] { color:#5fc97a; border-color:#3b7a4c; background:#1d2c22; }"
        "QLabel#pill[state=\"busy\"] { color:#f2b84b; border-color:#8a6a2a; background:#2f2616; }"
        "QLabel#pill[state=\"bad\"] { color:#ff8f82; border-color:#8a3a32; background:#2f1a17; }"
        "QSplitter::handle { background:#62626e; margin:1px; }"
        "QSplitter::handle:horizontal { width:5px; }"
        "QSplitter::handle:vertical { height:5px; }"
        "QSplitter::handle:hover { background:%1; }"
        "QHeaderView::section { background:#35353e; color:#e1e1e8; border:0; border-right:1px solid #555561;"
        " border-bottom:1px solid #747480; padding:4px 6px; }"
        "QMenu::separator { height:1px; background:#62626e; margin:5px 8px; }"
        "QStatusBar { color:#a0a0aa; }"
        "QStatusBar::item { border:0; }"
        "QDockWidget { font-weight:600; }"
        "QDockWidget::title { background:#25252b; padding:5px 8px; border-bottom:1px solid #62626e; }"
        "QLineEdit, QSpinBox, QComboBox { border-radius:4px; padding:3px 6px; }"
        // A button that is on shows it: the accent, not Fusion's barely darker shade
        "QPushButton:checked, QToolButton:checked { background:%1; color:%2; border:1px solid %1; border-radius:3px; }"
        // The main action, like NyxMap's GO
        "QPushButton[role=\"go\"] { background:#1f4a28; color:#f0f0f3; font-weight:bold; border:1px solid #3f9a52;"
        " border-radius:3px; padding:5px 14px; min-width:84px; }"
        "QPushButton[role=\"go\"]:hover { background:#27603a; }"
        "QPushButton[role=\"go\"]:disabled { background:#34343a; color:#78787f; border-color:#4a4a52; }"
        "QPlainTextEdit { background:#171a28; color:#d6d9e6; border:1px solid #62626e; }")
        .arg(css(), onAccent().name()));
}

} // namespace theme
