#pragma once

#include <QWidget>
#include <QPainter>
#include <QTimer>

// Self-contained SWR meter row: [Label | Bar with ticks | Numeric Value]
class SWRGauge : public QWidget {
    Q_OBJECT
public:
    explicit SWRGauge(QWidget *parent = nullptr);

    void setValue(double swr);
    void setAlarmPoint(double swr);
    void setPowerPresent(bool present) { m_powerPresent = present; update(); }
    void setTuneMode(bool tune) { m_tuneMode = tune; }
    void setPeakMode(bool peak) { m_peakMode = peak; }
    static void setDebugEnabled(bool on) { s_debugEnabled = on; }
    static bool s_debugEnabled;

    double value() const { return m_value; }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void decayValues();
    double swrToFraction(double swr) const;

    double m_value = 1.0;
    double m_displayValue = 0.0;
    double m_peakValue = 0.0;
    int m_peakHoldTicks = 0;
    double m_alarmPoint = 0.0;
    bool m_powerPresent = false;
    bool m_tuneMode = false;
    bool m_peakMode = false;
    double m_lastActiveSwr = 1.0;
    int m_swrHoldTicks = 0;         // Hold SWR display after power drops (Peak mode)
    int m_unityCount = 0;           // Consecutive SWR=1.00 readings (Peak mode filter)
    QTimer m_decayTimer;

    static constexpr int DecayIntervalMs = 50;
    static constexpr int PeakHoldTicks = 40;  // 2 sec hold (matches LP-100A default)
    // Decay rates — Tune is slow, Avg/Peak are fast
    // Avg/Peak: same decay rate for bar and peak indicator
    static constexpr double DecayFast = 0.22;
    static constexpr double PeakDecayFast = 0.22;   // Matches bar decay
    // Tune: slower decay
    static constexpr double DecaySlow = 0.08;
    static constexpr double PeakDecaySlow = 0.05;
};
