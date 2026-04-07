#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QScatterSeries>
#include "plot/SweepData.h"

class QStackedWidget;
class SweepEngine;
class SmithChartWidget;
class HamlibRig;
class LP100AProtocol;

class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(QWidget *parent = nullptr);

    enum DisplayMode {
        RplusJX, ZPhase, SwrMode, ReturnLoss, ReflCoeff, SmithChart
    };

    void setRig(HamlibRig *rig) { m_rig = rig; }
    void setLP100A(LP100AProtocol *lp100a);

    SweepData &sweepData() { return m_data; }
    double startFreqMHz() const;
    double stopFreqMHz() const;
    double stepKHz() const;

    void saveSettings();
    void loadSettings();

protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    void runRequested();
    void stopRequested();
    void resetRequested();
    void rigConnectRequested(int modelId, const QString &port, int baudRate);
    void rigDisconnectRequested();

private slots:
    void onDisplayModeChanged();
    void updateChart();
    void updateReadout(const SweepPoint &pt);

private:
    void createControls();
    void createChart();
    void addSeries(const QVector<double> &x, const QVector<double> &y,
                   const QColor &color, QValueAxis *yAxis);
    void addDualSeries(QChart *chart, QValueAxis *axisX, QValueAxis *axisY,
                       const QVector<double> &x, const QVector<double> &y,
                       const QColor &color, const QString &name = {});
    void plotSWR();
    void plotRplusJX();
    void plotZPhase();
    void plotReturnLoss();
    void plotReflCoeff();

    // Chart (QStackedWidget swaps between single/dual/Smith)
    QStackedWidget *m_chartStack;
    SmithChartWidget *m_smithChart;

    // Single chart (SWR, RL, ReflCoeff)
    QChartView *m_chartView;
    QChart *m_chart;
    QValueAxis *m_axisX;
    QValueAxis *m_axisY1;
    QValueAxis *m_axisY2;

    // Dual chart container (Z/Phase, R+jX) — two charts stacked vertically
    QWidget *m_dualChartWidget;
    QLabel *m_dualTitle;
    QChartView *m_chartViewTop;
    QChartView *m_chartViewBot;
    QChart *m_chartTop;
    QChart *m_chartBot;
    QValueAxis *m_axisXTop;
    QValueAxis *m_axisXBot;
    QValueAxis *m_axisYTop;
    QValueAxis *m_axisYBot;

    // Row 1: sweep parameters
    QDoubleSpinBox *m_startFreq;
    QDoubleSpinBox *m_stopFreq;
    QComboBox *m_stepCombo;
    QComboBox *m_displayCombo;
    QPushButton *m_runBtn;
    QPushButton *m_resetBtn;
    QPushButton *m_stopBtn;
    QCheckBox *m_signCheck;
    QCheckBox *m_splineCheck;
    QCheckBox *m_bestFitCheck;

    // Row 2: live readout
    QLabel *m_freqLabel;
    QLabel *m_pwrLabel;
    QLabel *m_zLabel;
    QLabel *m_phLabel;
    QLabel *m_rLabel;
    QLabel *m_xLabel;
    QLabel *m_swrLabel;

    // Row 3: setup (rig + sweep settings — like Windows Plot bottom row)
    QComboBox *m_sampleCombo;
    QComboBox *m_settleCombo;
    QComboBox *m_txModeCombo;
    QComboBox *m_rigCombo;
    QComboBox *m_rigPortCombo;
    QComboBox *m_rigBaudCombo;
    QDoubleSpinBox *m_powerSpin;
    QPushButton *m_rigConnectBtn;
    QPushButton *m_exportCsvBtn;
    QPushButton *m_exportImgBtn;

    // Row 4: raw LP-100A string
    QLabel *m_rawLabel;
    QLabel *m_statusLabel;

    // Data and sweep
    SweepData m_data;
    DisplayMode m_displayMode = SwrMode;
    SweepEngine *m_sweepEngine = nullptr;
    HamlibRig *m_rig = nullptr;
    LP100AProtocol *m_lp100a = nullptr;
    QList<QPair<int, QString>> m_rigList; // modelId, display name
};
