/****************************************************************************
** main.cpp
****************************************************************************/
#include <QApplication>
#include <QStyleFactory>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("VC3DProfileViewer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("NOTAVIS");

    // Dark style
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window,          QColor(40,  40,  42));
    darkPalette.setColor(QPalette::WindowText,       Qt::white);
    darkPalette.setColor(QPalette::Base,             QColor(28,  28,  30));
    darkPalette.setColor(QPalette::AlternateBase,    QColor(50,  50,  52));
    darkPalette.setColor(QPalette::ToolTipBase,      Qt::white);
    darkPalette.setColor(QPalette::ToolTipText,      Qt::white);
    darkPalette.setColor(QPalette::Text,             Qt::white);
    darkPalette.setColor(QPalette::Button,           QColor(55,  55,  58));
    darkPalette.setColor(QPalette::ButtonText,       Qt::white);
    darkPalette.setColor(QPalette::BrightText,       Qt::red);
    darkPalette.setColor(QPalette::Link,             QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight,        QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText,  Qt::black);
    app.setPalette(darkPalette);

    app.setStyleSheet(
        "QGroupBox { border: 1px solid #555; border-radius: 4px; margin-top: 6px; "
        "            font-weight: bold; padding-top: 4px; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; "
        "                   padding: 0 4px 0 4px; color: #aaa; } "
        "QLineEdit, QDoubleSpinBox { background: #222; border: 1px solid #555; "
        "                            color: white; padding: 2px 4px; border-radius: 3px; } "
        "QLabel { color: #ddd; } "
        "QStatusBar { background: #222; color: #888; }"
    );

    MainWindow w;
    w.show();
    return app.exec();
}
