#include "mainwindow.h"
#include <QtWidgets/QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Qt 内置 Fusion 风格 + 暗色调色板
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(53, 53, 53));
    dark.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    dark.setColor(QPalette::Base,            QColor(42, 42, 42));
    dark.setColor(QPalette::AlternateBase,   QColor(66, 66, 66));
    dark.setColor(QPalette::ToolTipBase,     QColor(53, 53, 53));
    dark.setColor(QPalette::ToolTipText,     QColor(220, 220, 220));
    dark.setColor(QPalette::Text,            QColor(220, 220, 220));
    dark.setColor(QPalette::Button,          QColor(53, 53, 53));
    dark.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    dark.setColor(QPalette::BrightText,      Qt::red);
    dark.setColor(QPalette::Link,            QColor(42, 130, 218));
    dark.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    dark.setColor(QPalette::HighlightedText, Qt::black);

    dark.setColor(QPalette::Disabled, QPalette::Text,       QColor(127, 127, 127));
    dark.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    dark.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));

    app.setPalette(dark);

    MainWindow window;
    window.show();
    return app.exec();
}
