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
    double m_lastActiveSwr = 1.0;  // Holds last SWR when power was present (for Tune mode)
    QTimer m_decayTimer;

    static constexpr int DecayIntervalMs = 50;
    static constexpr int PeakHoldTicks = 10;
    static constexpr double DecayRate = 0.08;
    static constexpr double PeakDecayRate = 0.04;
};
