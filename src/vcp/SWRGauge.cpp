#include "vcp/SWRGauge.h"
#include "Style.h"
#include <cmath>

SWRGauge::SWRGauge(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&m_decayTimer, &QTimer::timeout, this, &SWRGauge::decayValues);
    m_decayTimer.start(DecayIntervalMs);
}

QSize SWRGauge::minimumSizeHint() const {
    return QSize(Style::Layout::MeterMinWidth, Style::Layout::MeterRowHeight);
}

QSize SWRGauge::sizeHint() const {
    return QSize(500, Style::Layout::MeterRowHeight);
}

void SWRGauge::setValue(double swr) {
    m_value = qBound(Style::SWR::Min, swr, Style::SWR::Max);
    double ratio = swrToFraction(m_value);
    if (ratio > m_displayValue) m_displayValue = ratio;
    if (ratio > m_peakValue) {
        m_peakValue = ratio;
        m_peakHoldTicks = PeakHoldTicks;
    }
    update();
}

void SWRGauge::setAlarmPoint(double swr) {
    m_alarmPoint = swr;
    update();
}

double SWRGauge::swrToFraction(double swr) const {
    if (swr <= Style::SWR::Min) return 0.0;
    if (swr >= Style::SWR::Max) return 1.0;
    return std::log10(swr);
}

void SWRGauge::decayValues() {
    bool changed = false;
    double target = swrToFraction(m_value);
    if (m_displayValue > target) {
        m_displayValue -= DecayRate;
        if (m_displayValue < target) m_displayValue = target;
        changed = true;
    }
    if (m_peakValue > m_displayValue) {
        if (m_peakHoldTicks > 0) { m_peakHoldTicks--; }
        else {
            m_peakValue -= PeakDecayRate;
            if (m_peakValue < m_displayValue) m_peakValue = m_displayValue;
        }
        changed = true;
    }
    if (changed) update();
}

void SWRGauge::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int h = height();
    const int w = width();

    constexpr int labelBoxW = 36;
    constexpr int valueW = 80;
    constexpr int gap = 4;
    constexpr int barH = 12;
    constexpr int tickH = 2;

    const int barX = labelBoxW + gap;
    const int barW = w - barX - gap - valueW;
    const int barY = (h - barH) / 2 - 3;

    // --- Label box (fills vertical space like QK4) ---
    QRect labelRect(1, 1, labelBoxW - 2, h - 4);
    p.setPen(QColor(Style::Color::InactiveGray));
    p.setBrush(QColor(Style::Color::Background));
    p.drawRect(labelRect);

    QFont labelFont;
    labelFont.setPixelSize(Style::Font::Medium);
    labelFont.setBold(true);
    p.setFont(labelFont);
    p.setPen(QColor(Style::Color::TextWhite));
    p.drawText(labelRect, Qt::AlignCenter, "SWR");

    // --- Bar track ---
    QRect trackRect(barX, barY, barW, barH);
    p.fillRect(trackRect, QColor(Style::Color::Background));
    p.setPen(QColor(Style::Color::InactiveGray));
    p.drawRect(trackRect);

    // --- Filled bar ---
    if (m_displayValue > 0.001) {
        int fillW = static_cast<int>(barW * m_displayValue);
        QLinearGradient grad = Style::meterGradient(barX, 0, barX + barW, 0);
        p.fillRect(barX + 1, barY + 1, fillW - 2, barH - 2, grad);
    }

    // --- Peak hold ---
    if (m_peakValue > 0.01) {
        int peakX = barX + static_cast<int>(barW * m_peakValue);
        p.setPen(QPen(QColor(Style::Color::TextWhite), 1));
        p.drawLine(peakX, barY + 1, peakX, barY + barH - 1);
    }

    // --- Alarm marker ---
    if (m_alarmPoint > Style::SWR::Min) {
        double af = swrToFraction(m_alarmPoint);
        int ax = barX + static_cast<int>(barW * af);
        p.setPen(QPen(QColor(Style::Color::StatusRed), 2));
        p.drawLine(ax, barY, ax, barY + barH);
    }

    // --- Scale ticks ---
    const double swrValues[] = {1.0, 1.5, 2.0, 2.5, 3.0, 5.0, 10.0};
    const char *swrLabels[] = {"1", "1.5", "2", "2.5", "3", "5", "\xe2\x88\x9e"};
    constexpr int numTicks = 7;

    QFont scaleFont;
    scaleFont.setPixelSize(Style::Font::Tiny);
    p.setFont(scaleFont);

    int scaleY = barY + barH + 1;
    for (int i = 0; i < numTicks; ++i) {
        double frac = swrToFraction(swrValues[i]);
        int x = barX + static_cast<int>(barW * frac);

        p.setPen(QColor(Style::Color::InactiveGray));
        p.drawLine(x, barY, x, barY + tickH);

        p.setPen(swrValues[i] >= Style::SWR::WarningLimit
                 ? QColor(Style::Color::StatusRed) : QColor(Style::Color::TextGray));
        int textW = 24;
        int labelX = x - textW / 2;
        if (i == 0) labelX = x - 2;
        if (i == numTicks - 1) labelX = x - textW + 2;
        p.drawText(labelX, scaleY, textW, 8, Qt::AlignCenter, swrLabels[i]);
    }

    // --- Numeric value (right side) ---
    QRect valueRect(w - valueW, 0, valueW, h);
    QFont valueFont(Style::Font::Monospace);
    valueFont.setPixelSize(Style::Font::Large);
    valueFont.setBold(true);
    p.setFont(valueFont);

    // Color by SWR level
    if (m_value < Style::SWR::GoodLimit)
        p.setPen(QColor(Style::Color::StatusGreen));
    else if (m_value < Style::SWR::WarningLimit)
        p.setPen(QColor(Style::Color::MeterYellow));
    else
        p.setPen(QColor(Style::Color::StatusRed));

    p.drawText(valueRect, Qt::AlignRight | Qt::AlignVCenter,
               QString::number(m_value, 'f', 2));
}
