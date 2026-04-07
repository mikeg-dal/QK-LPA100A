#pragma once

#include <QWidget>
#include <QPainter>
#include <QTimer>

// Self-contained meter row: [Label | Bar with ticks | Numeric Value]
// All rendering in one widget — no separate labels needed.
class PowerGauge : public QWidget {
    Q_OBJECT
public:
    explicit PowerGauge(QWidget *parent = nullptr);

    void setValue(double watts);
    void setMaxValue(double max);
    void setLabel(const QString &label) { m_label = label; update(); }
    void setSuffix(const QString &suffix) { m_suffix = suffix; update(); }
    void setModeSuffix(const QString &s) { m_modeSuffix = s; update(); }
    void setPeakMode(bool peak) { m_peakMode = peak; }

    double value() const { return m_targetWatts; }
    double maxValue() const { return m_maxValue; }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void decayValues();
    double wattsToFrac(double w) const;

    double m_targetWatts = 0.0;
    double m_displayWatts = 0.0;
    double m_peakWatts = 0.0;
    int m_peakHoldTicks = 0;
    double m_maxValue = 25.0;
    QString m_label = "Pwr";
    QString m_suffix = "W";
    QString m_modeSuffix;  // "w" for Avg, "W" for Peak, "T" for Tune
    bool m_peakMode = false;
    QTimer m_decayTimer;

    static constexpr int DecayIntervalMs = 50;
    static constexpr int PeakHoldTicks = 40;  // 2 sec hold (matches LP-100A default)
    static constexpr double DecayFraction = 0.25;      // Faster bar decay
    static constexpr double PeakDecayFraction = 0.15;   // Faster peak indicator decay
};
