#pragma once

#include <QObject>
#include <QTimer>
#include "plot/SweepData.h"

class HamlibRig;
class LP100AProtocol;

struct SweepParams {
    double startMHz = 28.0;
    double stopMHz = 29.7;
    double stepKHz = 50;
    int settleTimeMs = 1000;    // Delay after freq change BEFORE TX (for SteppIR etc)
    int sampleTimeMs = 1000;    // How long to TX and let LP-100A measure
    QString txMode = "CW";      // Mode to set on rig
    double rfPower = 0.10;      // 0.0-1.0 power level
};

class SweepEngine : public QObject {
    Q_OBJECT
public:
    SweepEngine(HamlibRig *rig, LP100AProtocol *lp100a, QObject *parent = nullptr);

    void start(const SweepParams &params);
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void pointMeasured(const SweepPoint &pt);
    void progress(int current, int total, double freqMHz);
    void sweepFinished();
    void sweepError(const QString &msg);
    void logMessage(const QString &msg);  // For tracing Hamlib commands

private:
    void nextStep();
    void waitSettle();
    void pttOn();
    void readData();
    void pttOff();

    HamlibRig *m_rig;
    LP100AProtocol *m_lp100a;

    SweepParams m_params;
    double m_currentFreqMHz = 0;
    int m_currentStep = 0;
    int m_totalSteps = 0;
    bool m_running = false;
    bool m_stopRequested = false;

    static constexpr int ReleaseTimeMs = 100;
};
