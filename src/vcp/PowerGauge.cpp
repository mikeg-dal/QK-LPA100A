#include "vcp/PowerGauge.h"
#include "Style.h"
#include <cmath>

PowerGauge::PowerGauge(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&m_decayTimer, &QTimer::timeout, this, &PowerGauge::decayValues);
    m_decayTimer.start(DecayIntervalMs);
}

QSize PowerGauge::minimumSizeHint() const {
    return QSize(Style::Layout::MeterMinWidth, Style::Layout::MeterRowHeight);
}

QSize PowerGauge::sizeHint() const {
    return QSize(Style::Layout::MeterPreferredWidth, Style::Layout::MeterRowHeight);
}

void PowerGauge::setValue(double watts) {
    m_targetWatts = qMax(0.0, watts);
    if (m_targetWatts > m_displayWatts)
        m_displayWatts = m_targetWatts;
    if (m_targetWatts > m_peakWatts) {
        m_peakWatts = m_targetWatts;
        m_peakHoldTicks = PeakHoldTicks;
    }
    update();
}

void PowerGauge::setMaxValue(double max) {
    if (max == m_maxValue) return;
    m_maxValue = max;
    m_displayWatts = m_targetWatts;
    m_peakWatts = m_targetWatts;
    m_peakHoldTicks = 0;
    update();
}

void PowerGauge::decayValues() {
    if (!isVisible()) return;

    bool changed = false;
    if (m_displayWatts > m_targetWatts + 0.01) {
        m_displayWatts -= (m_displayWatts - m_targetWatts) * DecayFraction;
        if (m_displayWatts < m_targetWatts) m_displayWatts = m_targetWatts;
        changed = true;
    }
    if (m_peakWatts > m_displayWatts + 0.01) {
        if (m_peakHoldTicks > 0) { m_peakHoldTicks--; }
        else {
            m_peakWatts -= (m_peakWatts - m_displayWatts) * PeakDecayFraction;
            if (m_peakWatts < m_displayWatts) m_peakWatts = m_displayWatts;
        }
        changed = true;
    }
    if (changed) update();
}

double PowerGauge::wattsToFrac(double w) const {
    if (w <= 0 || m_maxValue <= 0) return 0.0;
    double floor = m_maxValue / 1000.0;
    if (w < floor) w = floor;
    if (w > m_maxValue) w = m_maxValue;
    double logMin = std::log10(floor);
    double logMax = std::log10(m_maxValue);
    return (std::log10(w) - logMin) / (logMax - logMin);
}

void PowerGauge::paintEvent(QPaintEvent *) {
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

    double displayRatio = wattsToFrac(m_displayWatts);
    double peakRatio    = wattsToFrac(m_peakWatts);

    // Label box (fills vertical space)
    QRect labelRect(1, 1, labelW - 2, h - 4);
    p.setPen(QColor(Style::Color::InactiveGray));
    p.setBrush(QColor(Style::Color::Background));
    p.drawRect(labelRect);

    QFont labelFont;
    labelFont.setPixelSize(Style::Font::Medium);
    labelFont.setBold(true);
    p.setFont(labelFont);
    p.setPen(QColor(Style::Color::TextWhite));
    p.drawText(labelRect, Qt::AlignCenter, m_label);

    // Bar track
    QRect trackRect(barX, barY, barW, barH);
    p.fillRect(trackRect, QColor(Style::Color::Background));
    p.setPen(QColor(Style::Color::InactiveGray));
    p.drawRect(trackRect);

    // Filled bar
    if (displayRatio > Style::Layout::MinBarFraction) {
        int fillW = static_cast<int>(barW * displayRatio);
        QLinearGradient grad = Style::meterGradient(barX, 0, barX + barW, 0);
        p.fillRect(barX + 1, barY + 1, fillW - 2, barH - 2, grad);
    }

    // Peak hold
    if (peakRatio > Style::Layout::MinPeakFraction) {
        int peakX = barX + static_cast<int>(barW * peakRatio);
        p.setPen(QPen(QColor(Style::Color::TextWhite), 1));
        p.drawLine(peakX, barY + 1, peakX, barY + barH - 1);
    }

    // Scale ticks + labels (log-positioned)
    struct Tick { double watts; const char *label; };
    QVector<Tick> ticks;
    if (m_maxValue <= Style::PowerRange::Low)
        ticks = {{0.1,".1"}, {0.5,".5"}, {1,"1"}, {2,"2"}, {5,"5"}, {10,"10"}, {25,"25"}};
    else if (m_maxValue <= Style::PowerRange::Mid)
        ticks = {{1,"1"}, {5,"5"}, {10,"10"}, {25,"25"}, {50,"50"}, {100,"100"}, {250,"250"}};
    else
        ticks = {{10,"10"}, {50,"50"}, {100,"100"}, {250,"250"}, {500,"500"}, {1000,"1K"}, {2500,"2.5K"}};

    QFont scaleFont;
    scaleFont.setPixelSize(Style::Font::Tiny);
    p.setFont(scaleFont);

    int scaleY = barY + barH + 1;
    for (int i = 0; i < ticks.size(); ++i) {
        double frac = wattsToFrac(ticks[i].watts);
        int x = barX + static_cast<int>(barW * frac);

        p.setPen(QColor(Style::Color::InactiveGray));
        p.drawLine(x, barY, x, barY + tickH);

        p.setPen(QColor(Style::Color::TextGray));
        int tw = Style::Layout::MeterScaleLabelW;
        int labelX = x - tw / 2;
        if (i == 0) labelX = x - 2;
        if (i == ticks.size() - 1) labelX = x - tw + 2;
        p.drawText(labelX, scaleY, tw, Style::Layout::MeterScaleLabelH,
                   Qt::AlignCenter, ticks[i].label);
    }

    // Numeric value (right side)
    QRect valueRect(w - valueW, 0, valueW, h);
    QFont valueFont(Style::Font::Monospace);
    valueFont.setPixelSize(Style::Font::Large);
    valueFont.setBold(true);
    p.setFont(valueFont);
    p.setPen(QColor(Style::Color::TextWhite));

    QString valueText;
    if (m_targetWatts >= 1000)
        valueText = QString::number(m_targetWatts, 'f', 0) + m_suffix;
    else if (m_targetWatts >= 100)
        valueText = QString::number(m_targetWatts, 'f', 1) + m_suffix;
    else
        valueText = QString::number(m_targetWatts, 'f', 2) + m_suffix;

    p.drawText(valueRect, Qt::AlignRight | Qt::AlignVCenter, valueText);
}

void PowerGauge::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    m_decayTimer.start(DecayIntervalMs);
}

void PowerGauge::hideEvent(QHideEvent *event) {
    m_decayTimer.stop();
    QWidget::hideEvent(event);
}
