#include "vcp/SWRGauge.h"
#include "Style.h"
#include <QDebug>
#include <cmath>

SWRGauge::SWRGauge(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&m_decayTimer, &QTimer::timeout, this, &SWRGauge::decayValues);
    m_decayTimer.start(DecayIntervalMs);
}

QSize SWRGauge::minimumSizeHint() const {
    return QSize(Style::Layout::MeterMinWidth, Style::Layout::MeterRowHeight);
}

QSize SWRGauge::sizeHint() const {
    return QSize(Style::Layout::MeterPreferredWidth, Style::Layout::MeterRowHeight);
}

void SWRGauge::setValue(double swr) {
    if (m_powerPresent) {
        // Reject SWR≈1.00 in ALL modes when we have a valid captured value.
        // The device resets SWR to 1.00 immediately after TX stops, but may
        // still report power (peak-held in Peak mode, or residual in others).
        // SWR=1.00 with held/residual power is NOT a real measurement.
        if (swr < 1.005) {
            if (m_lastActiveSwr > 1.01 && m_swrHoldTicks > 0) {
                qDebug() << "[SWR] REJECT swr=" << swr << "keeping" << m_lastActiveSwr
                         << "holdTicks=" << m_swrHoldTicks;
                update();
                return;
            }
            // No capture yet — count consecutive 1.00 readings. Only accept after 3+
            // consecutive polls (meaning it's truly a matched load, not a timing artifact)
            m_unityCount++;
            if (m_unityCount < 10) {  // Require ~800ms of continuous SWR=1.00 with power
                qDebug() << "[SWR] DEFER swr=1.00 unityCount=" << m_unityCount;
                update();
                return;
            }
        } else {
            m_unityCount = 0;  // Reset counter when we see non-unity SWR
        }

        qDebug() << "[SWR] ACCEPT swr=" << swr << "powerPresent=" << m_powerPresent
                 << "peak=" << m_peakMode << "holdTicks=" << m_swrHoldTicks;

        m_value = qBound(Style::SWR::Min, swr, Style::SWR::Max);
        m_lastActiveSwr = m_value;
        m_swrHoldTicks = PeakHoldTicks;
        double ratio = swrToFraction(m_value);
        if (ratio > m_displayValue) m_displayValue = ratio;
        if (ratio > m_peakValue) {
            m_peakValue = ratio;
            m_peakHoldTicks = PeakHoldTicks;
        }
    }
    update();
}

void SWRGauge::setAlarmPoint(double swr) {
    m_alarmPoint = swr;
    update();
}

double SWRGauge::swrToFraction(double swr) const {
    // Normalized log scale: maps [SWR::Min, SWR::Max] to [0, 1]
    if (swr <= Style::SWR::Min) return 0.0;
    if (swr >= Style::SWR::Max) return 1.0;
    return std::log10(swr) / std::log10(Style::SWR::Max);
}

void SWRGauge::decayValues() {
    if (!isVisible()) return;

    bool changed = false;
    double target = swrToFraction(m_value);

    // Proportional decay (matches PowerGauge behavior)
    if (m_displayValue > target + 0.001) {
        m_displayValue -= (m_displayValue - target) * DecayRate;
        if (m_displayValue < target) m_displayValue = target;
        changed = true;
    }
    if (m_peakValue > m_displayValue + 0.001) {
        if (m_peakHoldTicks > 0) { m_peakHoldTicks--; }
        else {
            m_peakValue -= (m_peakValue - m_displayValue) * PeakDecayRate;
            if (m_peakValue < m_displayValue) m_peakValue = m_displayValue;
        }
        changed = true;
    }

    // Count down SWR numeric hold timer (Avg/Peak modes).
    // Tune mode: never count down — hold permanently.
    if (m_swrHoldTicks > 0 && !m_powerPresent && !m_tuneMode) {
        m_swrHoldTicks--;
        changed = true;
    }
    if (changed) update();
}

void SWRGauge::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int h = height();
    const int w = width();
    const int labelW  = Style::Layout::MeterLabelBoxWidth;
    const int valueW  = Style::Layout::MeterValueWidth;
    const int barGap  = Style::Layout::MeterBarGap;
    const int barH    = Style::Layout::MeterBarHeight;
    const int tickH   = Style::Layout::MeterTickHeight;
    const int barX    = labelW + barGap;
    const int barW    = w - barX - barGap - valueW;
    const int barY    = (h - barH) / 2 - 3;

    // Label box
    QRect labelRect(1, 1, labelW - 2, h - 4);
    p.setPen(QColor(Style::Color::InactiveGray));
    p.setBrush(QColor(Style::Color::Background));
    p.drawRect(labelRect);

    QFont labelFont;
    labelFont.setPixelSize(Style::Font::Medium);
    labelFont.setBold(true);
    p.setFont(labelFont);
    p.setPen(QColor(Style::Color::TextWhite));
    p.drawText(labelRect, Qt::AlignCenter, "SWR");

    // Bar track
    QRect trackRect(barX, barY, barW, barH);
    p.fillRect(trackRect, QColor(Style::Color::Background));
    p.setPen(QColor(Style::Color::InactiveGray));
    p.drawRect(trackRect);

    // Filled bar
    if (m_displayValue > Style::Layout::MinBarFraction) {
        int fillW = static_cast<int>(barW * m_displayValue);
        QLinearGradient grad = Style::meterGradient(barX, 0, barX + barW, 0);
        p.fillRect(barX + 1, barY + 1, fillW - 2, barH - 2, grad);
    }

    // Peak hold
    if (m_peakValue > Style::Layout::MinPeakFraction) {
        int peakX = barX + static_cast<int>(barW * m_peakValue);
        p.setPen(QPen(QColor(Style::Color::TextWhite), 1));
        p.drawLine(peakX, barY + 1, peakX, barY + barH - 1);
    }

    // Alarm marker
    if (m_alarmPoint > Style::SWR::Min) {
        double af = swrToFraction(m_alarmPoint);
        int ax = barX + static_cast<int>(barW * af);
        p.setPen(QPen(QColor(Style::Color::StatusRed), 2));
        p.drawLine(ax, barY, ax, barY + barH);
    }

    // Scale ticks (log-positioned)
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
        int tw = Style::Layout::MeterScaleLabelW;
        int labelX = x - tw / 2;
        if (i == 0) labelX = x - 2;
        if (i == numTicks - 1) labelX = x - tw + 2;
        p.drawText(labelX, scaleY, tw, Style::Layout::MeterScaleLabelH,
                   Qt::AlignCenter, swrLabels[i]);
    }

    // Numeric value (right side, color-coded)
    QRect valueRect(w - valueW, 0, valueW, h);
    QFont valueFont(Style::Font::Monospace);
    valueFont.setPixelSize(Style::Font::Large);
    valueFont.setBold(true);
    p.setFont(valueFont);

    // SWR numeric display — mode-specific behavior:
    //
    // ALL MODES during TX: show current SWR
    // AVERAGE ("w") after TX: hold last SWR for peak hold period, then "-.--"
    // PEAK ("W") after TX:   hold last SWR for peak hold period, then "-.--"
    // TUNE ("T") after TX:   hold last SWR permanently (tuning reference)
    //
    QString swrText;
    double displaySwr = m_lastActiveSwr;
    bool showValue = false;

    if (m_powerPresent) {
        // Actively transmitting — all modes show current SWR
        displaySwr = m_value;
        showValue = true;
    } else if (m_tuneMode) {
        // Tune: hold last value permanently until next TX
        displaySwr = m_lastActiveSwr;
        showValue = (m_lastActiveSwr > 1.0);
    } else if (m_swrHoldTicks > 0) {
        // Avg and Peak: hold during peak hold period, then "-.--"
        displaySwr = m_lastActiveSwr;
        showValue = (m_lastActiveSwr > 1.0);
    }
    // Hold expired with no power: showValue stays false → "-.--"

    if (showValue) {
        swrText = QString::number(displaySwr, 'f', 2);
    } else {
        swrText = "-.--";
    }

    if (!showValue) {
        p.setPen(QColor(Style::Color::TextGray));
    } else if (displaySwr < Style::SWR::GoodLimit) {
        p.setPen(QColor(Style::Color::StatusGreen));
    } else if (displaySwr < Style::SWR::WarningLimit) {
        p.setPen(QColor(Style::Color::MeterYellow));
    } else {
        p.setPen(QColor(Style::Color::StatusRed));
    }

    p.drawText(valueRect, Qt::AlignRight | Qt::AlignVCenter, swrText);
}

void SWRGauge::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    m_decayTimer.start(DecayIntervalMs);
}

void SWRGauge::hideEvent(QHideEvent *event) {
    m_decayTimer.stop();
    QWidget::hideEvent(event);
}
