#include "About.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
struct Credit { const char *name, *url, *license; };

const Credit credits[] = {
    {"Qt 6", "https://www.qt.io", "LGPL-3.0, the interface and the network"},
    {"Blackmagic ATEM Switchers SDK", "https://www.blackmagicdesign.com/developer/products/atem/sdk-and-software",
     "Blackmagic Design, the connection to the switchers"},
    {"atemOSC", "https://github.com/SteffeyDev/atemOSC", "by Peter Steffey and contributors, where this started"},
    {"NyxMap", "https://github.com/PixelStereo/NyxMap", "the look of the interface"},
};
} // namespace

void showAbout(QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("About AtemRemoteOsc"));
    auto *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral(
        "<h2 style='margin:0'>AtemRemoteOsc</h2><p style='margin:2px 0'>OSC control of ATEM switchers · %1</p>"
        "<p style='margin:6px 0 0'>Drive a Blackmagic ATEM from any OSC controller: every address answers with its value, "
        "and OSCQuery clients find it by themselves.<br>Built by <a href=\"https://pixelstereo.com\">Pixel Stereo</a>.</p>")
                                 .arg(QApplication::applicationVersion()));
    title->setTextFormat(Qt::RichText);
    title->setOpenExternalLinks(true);
    title->setWordWrap(true);
    layout->addWidget(title);

    QString html = QStringLiteral(
        "<h3 style='margin:0 0 4px'>Source</h3>"
        "<p style='margin:0'>Open source, on <a href=\"https://github.com/PixelStereo/AtemRemoteOSC\">GitHub</a>, "
        "without any warranty.</p>"
        "<h3 style='margin:12px 0 4px'>Credits</h3><ul style='margin:0'>");
    for (const auto &credit : credits)
        html += QStringLiteral("<li><a href=\"%1\">%2</a>, %3</li>").arg(QString::fromLatin1(credit.url), QString::fromLatin1(credit.name),
                                                                       QString::fromLatin1(credit.license));
    html += QStringLiteral("</ul>");
    auto *details = new QLabel(html);
    details->setTextFormat(Qt::RichText);
    details->setOpenExternalLinks(true);
    details->setWordWrap(true);
    details->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto *scroll = new QScrollArea;
    scroll->setWidget(details);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    layout->addWidget(scroll, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(500, 400);
    dialog.exec();
}
