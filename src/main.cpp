#include <QApplication>
#include <QPalette>
#include "Style.h"
#include "vcp/VCPMainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("QK-LP100A");
    app.setOrganizationName("AI5QK");
    app.setApplicationVersion("0.1.0");

    // QK4-style neutral dark palette
    QPalette pal;
    pal.setColor(QPalette::Window,          QColor(Style::Color::Background));
    pal.setColor(QPalette::WindowText,      QColor(Style::Color::TextWhite));
    pal.setColor(QPalette::Base,            QColor(Style::Color::DarkBackground));
    pal.setColor(QPalette::AlternateBase,   QColor(Style::Color::Background));
    pal.setColor(QPalette::ToolTipBase,     QColor(Style::Color::Surface));
    pal.setColor(QPalette::ToolTipText,     QColor(Style::Color::TextWhite));
    pal.setColor(QPalette::Text,            QColor(Style::Color::TextWhite));
    pal.setColor(QPalette::Button,          QColor("#3a3a3a"));
    pal.setColor(QPalette::ButtonText,      QColor(Style::Color::TextWhite));
    pal.setColor(QPalette::BrightText,      QColor(Style::Color::StatusRed));
    pal.setColor(QPalette::Link,            QColor(Style::Color::AccentCyan));
    pal.setColor(QPalette::Highlight,       QColor(Style::Color::AccentAmber));
    pal.setColor(QPalette::HighlightedText, QColor(Style::Color::TextDark));
    app.setPalette(pal);

    VCPMainWindow window;
    window.show();

    return app.exec();
}
