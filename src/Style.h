#pragma once

#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QString>

// Centralized style constants — follows QK4 styling conventions.
// No magic numbers in UI code. All values referenced from here.

namespace Style {

// =============================================================================
// Color Palette (matching QK4 neutral dark theme)
// =============================================================================
namespace Color {
    // Backgrounds
    constexpr const char *Background     = "#1a1a1a";
    constexpr const char *DarkBackground = "#0d0d0d";
    constexpr const char *Surface        = "#1e1e1e";

    // Accent
    constexpr const char *AccentAmber    = "#FFB000";
    constexpr const char *AccentCyan     = "#00BFFF";

    // Text
    constexpr const char *TextWhite      = "#FFFFFF";
    constexpr const char *TextDark       = "#333333";
    constexpr const char *TextGray       = "#999999";
    constexpr const char *InactiveGray   = "#666666";

    // Meter gradient stops
    constexpr const char *MeterGreenDark  = "#00CC00";
    constexpr const char *MeterGreen      = "#00FF00";
    constexpr const char *MeterYellowGreen= "#CCFF00";
    constexpr const char *MeterYellow     = "#FFFF00";
    constexpr const char *MeterOrange     = "#FF6600";
    constexpr const char *MeterOrangeRed  = "#FF3300";
    constexpr const char *MeterRed        = "#FF0000";

    // Status
    constexpr const char *StatusGreen    = "#00FF00";
    constexpr const char *StatusRed      = "#FF0000";

    // Borders
    constexpr const char *BorderNormal   = "#606060";
    constexpr const char *BorderHover    = "#808080";
    constexpr const char *PanelBorder    = "#444444";
}

// =============================================================================
// Font
// =============================================================================
namespace Font {
    constexpr const char *Family    = "Inter";
    constexpr const char *Monospace = "Menlo";

    constexpr int Tiny       = 7;    // QK4: FontSizeTiny
    constexpr int Small      = 8;    // QK4: FontSizeSmall
    constexpr int Normal     = 9;    // QK4: FontSizeNormal
    constexpr int Medium     = 10;   // QK4: FontSizeMedium
    constexpr int Button     = 11;   // QK4: FontSizeLarge
    constexpr int Large      = 12;   // QK4: FontSizeButton
    constexpr int Callsign   = 16;
    constexpr int SWRReadout = 18;
    constexpr int PowerReadout = 28;
}

// =============================================================================
// Layout (pixels)
// =============================================================================
namespace Layout {
    constexpr int WindowMinWidth  = 520;
    constexpr int WindowMinHeight = 420;
    constexpr int MainMargin      = 12;
    constexpr int MainSpacing     = 6;

    constexpr int ButtonHeight    = 36;
    constexpr int BorderRadius    = 6;

    constexpr int MeterRowHeight  = 28;  // Room for bar + scale labels below
    constexpr int MeterLabelWidth = 40;
    constexpr int MeterBarHeight  = 14;
    constexpr int MeterBarGap     = 4;
    constexpr int MeterTickHeight = 2;
    constexpr int MeterMinWidth   = 300;

    constexpr int BannerPad       = 6;
    constexpr int BannerRadius    = 4;
    constexpr int ImpGridHSpacing = 16;
    constexpr int ImpGridVSpacing = 4;
    constexpr int SeparatorHeight = 1;
}

// =============================================================================
// Protocol Defaults
// =============================================================================
namespace Protocol {
    constexpr int DefaultBaud    = 115200;
    constexpr int DefaultPollMs  = 80;
    constexpr int MinPollMs      = 50;
    constexpr int MaxPollMs      = 5000;
    constexpr int PollStep       = 10;
    constexpr int DefaultTcpPort = 10001;
    constexpr int MinTcpPort     = 1;
    constexpr int MaxTcpPort     = 65535;
}

// =============================================================================
// Power Ranges (watts)
// =============================================================================
namespace PowerRange {
    constexpr double Low  = 25.0;
    constexpr double Mid  = 250.0;
    constexpr double High = 2500.0;
}

// =============================================================================
// SWR Thresholds
// =============================================================================
namespace SWR {
    constexpr double Min          = 1.0;
    constexpr double Max          = 10.0;
    constexpr double GoodLimit    = 1.5;
    constexpr double WarningLimit = 3.0;
    constexpr double AlarmPowerThreshold = 0.5;
}

// =============================================================================
// Meter Gradient (QK4-style green->yellow->orange->red)
// =============================================================================
inline QLinearGradient meterGradient(qreal x1, qreal y1, qreal x2, qreal y2) {
    QLinearGradient g(x1, y1, x2, y2);
    g.setColorAt(0.00, QColor(Color::MeterGreenDark));
    g.setColorAt(0.15, QColor(Color::MeterGreen));
    g.setColorAt(0.30, QColor(Color::MeterYellowGreen));
    g.setColorAt(0.45, QColor(Color::MeterYellow));
    g.setColorAt(0.60, QColor(Color::MeterOrange));
    g.setColorAt(0.80, QColor(Color::MeterOrangeRed));
    g.setColorAt(1.00, QColor(Color::MeterRed));
    return g;
}

// =============================================================================
// CSS Helpers — following QK4 pattern: helper to build gradient strings,
// then compose complete stylesheets from those building blocks.
// =============================================================================
namespace detail {

// 4-stop vertical gradient CSS string (QK4 button pattern)
inline QString gradient(const char *top, const char *mid1, const char *mid2, const char *bottom) {
    return QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                          "stop:0 ") + top +
           QStringLiteral(",stop:0.4 ") + mid1 +
           QStringLiteral(",stop:0.6 ") + mid2 +
           QStringLiteral(",stop:1 ") + bottom + QStringLiteral(")");
}

// Reversed gradient for pressed state
inline QString gradientReversed(const char *top, const char *mid1, const char *mid2, const char *bottom) {
    return gradient(bottom, mid2, mid1, top);
}

} // namespace detail

// Standard dark gradient button (QK4 popupButtonNormal equivalent)
inline QString buttonNormal() {
    return QStringLiteral("QPushButton { background: ") +
        detail::gradient("#4a4a4a", "#3a3a3a", "#353535", "#2a2a2a") +
        QStringLiteral("; color: #FFFFFF; border: 2px solid #606060;"
                       " border-radius: 6px; padding: 6px 12px;"
                       " font-size: 12px; font-weight: 600; }"
                       "QPushButton:hover { background: ") +
        detail::gradient("#5a5a5a", "#4a4a4a", "#454545", "#3a3a3a") +
        QStringLiteral("; border: 2px solid #808080; }"
                       "QPushButton:pressed { background: ") +
        detail::gradientReversed("#4a4a4a", "#3a3a3a", "#353535", "#2a2a2a") +
        QStringLiteral("; border: 2px solid #909090; }");
}

// Alarm tripped — solid red
inline QString buttonAlarmTripped() {
    return QStringLiteral(
        "QPushButton { background: #FF0000; color: #FFFFFF;"
        " border: 2px solid #FF0000; border-radius: 6px;"
        " padding: 6px 12px; font-size: 12px; font-weight: bold; }");
}

// Connect button — green gradient
inline QString connectButton() {
    return QStringLiteral("QPushButton { background: ") +
        detail::gradient("#2a5a2a", "#1a4a1a", "#184518", "#143814") +
        QStringLiteral("; color: #FFFFFF; border: 2px solid #3a6a3a;"
                       " border-radius: 6px; padding: 6px 12px;"
                       " font-size: 12px; font-weight: 600; }"
                       "QPushButton:hover { background: ") +
        detail::gradient("#3a6a3a", "#2a5a2a", "#255525", "#1a4a1a") +
        QStringLiteral("; border: 2px solid #4a7a4a; }");
}

// Disconnect button — red gradient
inline QString disconnectButton() {
    return QStringLiteral("QPushButton { background: ") +
        detail::gradient("#5a2a2a", "#4a1a1a", "#451818", "#381414") +
        QStringLiteral("; color: #FFFFFF; border: 2px solid #6a3a3a;"
                       " border-radius: 6px; padding: 6px 12px;"
                       " font-size: 12px; font-weight: 600; }"
                       "QPushButton:hover { background: ") +
        detail::gradient("#6a3a3a", "#5a2a2a", "#552525", "#4a1a1a") +
        QStringLiteral("; border: 2px solid #7a4a4a; }");
}

} // namespace Style
