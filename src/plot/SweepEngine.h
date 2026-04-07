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
    qint64 m_currentFreqHz = 0;   // Use integer Hz to avoid floating point drift
    qint64 m_startHz = 0;
    qint64 m_stopHz = 0;
    qint64 m_stepHz = 0;
    int m_currentStep = 0;
    int m_totalSteps = 0;
    bool m_running = false;
    bool m_stopRequested = false;

    // Saved rig state — restored after sweep
    double m_savedFreqHz = 0;
    QString m_savedMode;

    static constexpr int ReleaseTimeMs = 100;
};
