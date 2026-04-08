#include <QApplication>
#include <QPalette>
#include <QTcpSocket>
#include <QSettings>
#include "Style.h"
#include "vcp/VCPMainWindow.h"

// Pre-warm macOS network stack at startup.
// macOS blocks the first TCP SYN from a new process while evaluating it.
// By making a throwaway connection attempt at startup, the process gets
// cleared by the time the user actually tries to connect to a rig.
static void prewarmNetwork() {
    QSettings s("AI5QK", "QK-LP100A");
    QString lastRigPort = s.value("rig/port", "").toString();
    if (lastRigPort.contains(':')) {
        QStringList parts = lastRigPort.split(':');
        if (parts.size() == 2) {
            QTcpSocket warmup;
            warmup.connectToHost(parts[0], parts[1].toUShort());
            warmup.waitForConnected(1000);
            warmup.abort();
        }
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("QK-LP100A");
    app.setOrganizationName("AI5QK");
    app.setApplicationVersion(QKLP100A_VERSION);

    // Warm the macOS network stack early
    prewarmNetwork();

    // QK4-style neutral dark palette
    QPalette pal;
    pal.setColor(QPalette::Window,          QColor(Style::Color::Background));
    pal.setColor(QPalette::WindowText,      QColor(Style::Color::TextWhite));
    pal.setColor(QPalette::Base,            QColor(Style::Color::DarkBackground));
    pal.setColor(QPalette::AlternateBase,   QColor(Style::Color::Background));
    pal.setColor(QPalette::ToolTipBase,     QColor(Style::Color::Surface));
    pal.setColor(QPalette::ToolTipText,     QColor(Style::Color::TextWhite));
    pal.setColor(QPalette::Text,            QColor(Style::Color::TextWhite));
    pal.setColor(QPalette::Button,          QColor(Style::Color::GradientMid1));
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
