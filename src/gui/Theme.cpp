#include "Theme.h"
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

namespace theme {

namespace {
QColor g_accent;
bool g_loaded = false;
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

QString tabStyle()
{
    return QStringLiteral("QTabBar::tab { background:#25252b; color:#c3c3cd; padding:5px 9px; margin-right:3px;"
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
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
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
        "QGroupBox { font-weight: bold; border: 1px solid #62626e; border-radius: 4px; margin-top: 10px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: %1; }"
        "QSplitter::handle { background:#62626e; margin:1px; }"
        "QSplitter::handle:horizontal { width:7px; }"
        "QSplitter::handle:vertical { height:7px; }"
        "QSplitter::handle:hover { background:%1; }"
        "QHeaderView::section { background:#35353e; color:#e1e1e8; border:0; border-right:1px solid #555561;"
        " border-bottom:1px solid #747480; padding:4px 6px; }"
        "QMenu::separator { height:1px; background:#62626e; margin:5px 8px; }"
        "QToolBar { border:0; border-bottom:1px solid #62626e; spacing:6px; }"
        "QStatusBar { color:#a0a0aa; }"
        // A button that is on shows it: the accent, not Fusion's barely darker shade
        "QPushButton:checked, QToolButton:checked { background:%1; color:%2; border:1px solid %1; border-radius:3px; }"
        // The main action, like NyxMap's GO
        "QPushButton[role=\"go\"] { background:#1f4a28; color:#f0f0f3; font-weight:bold; border:1px solid #3f9a52; border-radius:3px; padding:4px 8px; min-width:72px; }"
        "QPushButton[role=\"go\"]:hover { background:#27603a; }"
        "QPushButton[role=\"go\"]:disabled { background:#34343a; color:#78787f; border-color:#4a4a52; }"
        "QPlainTextEdit { background:#171a28; color:#d6d9e6; border:1px solid #62626e; }")
        .arg(css(), onAccent().name()));
}

} // namespace theme
