#pragma once

#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QString>

// Centralized style constants — follows QK4 styling conventions.
// Every visual value in the app references this file. No magic numbers elsewhere.

namespace Style {

// =============================================================================
// Color Palette (QK4 neutral dark theme)
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

    // Meter gradient stops (green -> yellow -> red)
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

    // Button gradient stops (4-stop vertical, QK4 pattern)
    constexpr const char *GradientTop    = "#4a4a4a";
    constexpr const char *GradientMid1   = "#3a3a3a";
    constexpr const char *GradientMid2   = "#353535";
    constexpr const char *GradientBottom = "#2a2a2a";
    constexpr const char *HoverTop       = "#5a5a5a";
    constexpr const char *HoverMid1      = "#4a4a4a";
    constexpr const char *HoverMid2      = "#454545";
    constexpr const char *HoverBottom    = "#3a3a3a";

    // Borders
    constexpr const char *BorderNormal   = "#606060";
    constexpr const char *BorderHover    = "#808080";
    constexpr const char *BorderPressed  = "#909090";
    constexpr const char *PanelBorder    = "#444444";
}

// =============================================================================
// Font (pixel sizes — use with QFont::setPixelSize)
// =============================================================================
namespace Font {
    constexpr const char *Family    = "Inter";
    constexpr const char *Monospace = "Menlo";

    constexpr int Tiny       = 7;    // Scale ticks, raw label
    constexpr int Small      = 8;    // Status bar, secondary text
    constexpr int Normal     = 9;    // Field labels (Z:, R:, etc.)
    constexpr int Medium     = 10;   // Meter labels, impedance values, compact buttons
    constexpr int Large      = 12;   // Gauge inline readout
    constexpr int Callsign   = 16;   // Callsign banner
}

// =============================================================================
// Layout (pixels)
// =============================================================================
namespace Layout {
    // Main window margins and spacing
    constexpr int MainMarginH     = 8;
    constexpr int MainMarginTop   = 6;
    constexpr int MainMarginBottom= 4;
    constexpr int MainSpacing     = 2;   // Between meter rows

    // Callsign banner
    constexpr int BannerPadH      = 8;
    constexpr int BannerPadV      = 2;
    constexpr int BannerRadius    = 3;

    // Meter gauge geometry (shared by PowerGauge and SWRGauge)
    constexpr int MeterRowHeight     = 28;   // Total widget height per meter
    constexpr int MeterLabelBoxWidth = 36;   // "Pwr" / "Ref" / "SWR" label box
    constexpr int MeterBarHeight     = 12;   // Filled bar track height
    constexpr int MeterBarGap        = 4;    // Gap between label box and bar
    constexpr int MeterValueWidth    = 80;   // Numeric readout area on right
    constexpr int MeterTickHeight    = 2;    // Tick marks above bar
    constexpr int MeterScaleLabelW   = 24;   // Bounding box width per tick label
    constexpr int MeterScaleLabelH   = 8;    // Bounding box height per tick label
    constexpr int MeterPreferredWidth= 500;  // sizeHint preferred width
    constexpr int MeterMinWidth      = 300;  // minimumSizeHint width

    // Paint thresholds (minimum fraction to draw)
    constexpr double MinBarFraction  = 0.001;
    constexpr double MinPeakFraction = 0.01;

    // Buttons
    constexpr int CompactButtonHeight = 22;
    constexpr int ButtonSpacing       = 4;

    // Impedance grid
    constexpr int ImpGridHSpacing = 16;
    constexpr int ImpGridVSpacing = 2;
    constexpr int ImpGridMarginTop= 2;

    // Bottom elements
    constexpr int RawLabelHeight  = 14;
    constexpr int StatusBarHeight = 18;

    // Connection dialog
    constexpr int DialogMinWidth  = 380;
    constexpr int DialogSpacing   = 8;
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
    constexpr const char *DefaultTcpHost = "192.168.1.100";
    constexpr int CommandDelayMs = 150;  // Delay after A/M/F before polling (LP-100A needs time)
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
// CSS Helpers — gradient builder + button stylesheets
// =============================================================================
namespace detail {

inline QString gradient(const char *top, const char *mid1, const char *mid2, const char *bottom) {
    return QString("qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                   "stop:0 %1,stop:0.4 %2,stop:0.6 %3,stop:1 %4)")
        .arg(top, mid1, mid2, bottom);
}

inline QString gradientReversed(const char *top, const char *mid1, const char *mid2, const char *bottom) {
    return gradient(bottom, mid2, mid1, top);
}

} // namespace detail

// Compact control button (QK4 compactButton style)
inline QString buttonCompact() {
    static const QString s =
        QStringLiteral("QPushButton { background: ") +
        detail::gradient(Color::GradientTop, Color::GradientMid1,
                         Color::GradientMid2, Color::GradientBottom) +
        QString("; color: %1; border: 1px solid %2;"
                " border-radius: 4px; padding: 3px 8px;"
                " font-size: %3px; font-weight: bold; }")
            .arg(Color::TextWhite).arg(Color::BorderNormal)
            .arg(Font::Medium) +
        QStringLiteral("QPushButton:hover { background: ") +
        detail::gradient(Color::HoverTop, Color::HoverMid1,
                         Color::HoverMid2, Color::HoverBottom) +
        QString("; border: 1px solid %1; }").arg(Color::BorderHover) +
        QStringLiteral("QPushButton:pressed { background: ") +
        detail::gradientReversed(Color::GradientTop, Color::GradientMid1,
                                 Color::GradientMid2, Color::GradientBottom) +
        QString("; border: 1px solid %1; }").arg(Color::BorderPressed);
    return s;
}

// Compact alarm tripped
inline QString buttonCompactAlarm() {
    static const QString s =
        QString("QPushButton { background: %1; color: %2;"
                " border: 1px solid %1; border-radius: 4px;"
                " padding: 3px 8px; font-size: %3px; font-weight: bold; }")
            .arg(Color::StatusRed, Color::TextWhite)
            .arg(Font::Medium);
    return s;
}

} // namespace Style
